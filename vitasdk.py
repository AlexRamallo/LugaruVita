"""
	Support for building with VitaSDK https://vitasdk.org/
"""
import waflib, socket, time, os, threading, subprocess, shutil
from waflib import TaskGen, Task, Context
from waflib.Configure import conf
from ftplib import FTP
from os import path

def options(opt):
	opt.add_option('--SHARKIP', dest='SHARKIP', type='string', default=None)

def configure(conf):
	sdk_installed = "VITASDK" in conf.environ
	conf.msg(msg="Checking for VITASDK environment variable", result=sdk_installed)
	if not sdk_installed:
		return

	conf.load('compiler_cxx compiler_c')

	sysroot = conf.environ['VITASDK']
	prefix = 'arm-vita-eabi'

	conf.env.VITASDK = sysroot	
	conf.env.TOOLCHAIN_SYSROOT = sysroot
	conf.env.TOOLCHAIN_PREFIX = prefix

	conf.env.CC 		= [path.join(sysroot, 'bin', prefix + '-gcc')]
	conf.env.LINK_CC	= [path.join(sysroot, 'bin', prefix + '-gcc')]
	conf.env.CXX 		= [path.join(sysroot, 'bin', prefix + '-g++')]
	conf.env.LINK_CXX	= [path.join(sysroot, 'bin', prefix + '-g++')]
	conf.env.AR 		= [path.join(sysroot, 'bin', prefix + '-ar')]
	conf.env.PKGCONFIG	= [path.join(sysroot, 'bin', prefix + '-pkg-config')]

	conf.env.CC_NAME = "gcc"
	conf.env.CXX_NAME = "g++"
	conf.env.COMPILER_CC = "gcc"
	conf.env.COMPILER_CXX = "g++"

	conf.get_cc_version(conf.env.CC, gcc=True)

	conf.env.DEST_OS = "Vita"
	conf.env.DEST_CPU = "armv7hf"

	vitabinroot = ['%s/bin' % conf.env.VITASDK]

	conf.find_program('convert', var='MAGICK_CONVERT', mandatory=False)
	conf.find_program('zip', var='ZIP', mandatory=True)
	conf.find_program('curl', var='CURL', mandatory=True)
	conf.find_program('vita-mksfoex', var='VITA_MKSFOEX', path_list=vitabinroot, mandatory=True)
	conf.find_program('vita-elf-create', var='VITA_ELF_CREATE', path_list=vitabinroot, mandatory=True)
	conf.find_program('vita-make-fself', var='VITA_MAKE_FSELF', path_list=vitabinroot, mandatory=True)
	conf.find_program('arm-vita-eabi-strip', var='VITA_STRIP', path_list=vitabinroot, mandatory=True)


@TaskGen.feature("vita_eboot")
@TaskGen.after_method('apply_link')
def tsk_vita_eboot(self):
	t1 = self.create_task('create_eboot')
	t1.inputs = [self.path.find_or_declare(self.target)]
	t1.outputs = [self.path.get_bld().find_or_declare('eboot.bin')]


@TaskGen.feature("vita_vpk")
@TaskGen.after_method('apply_link')
def tsk_vita_vpk(self):
	title_id = self.vita_title_id
	title_string = self.vita_title_string

	t1 = self.create_task('create_eboot')
	t1.inputs = [self.path.find_or_declare(self.target)]
	t1.outputs = [self.path.get_bld().find_or_declare('eboot.bin')]
		
	sce_sys = self.path.find_node('sce_sys')
	if sce_sys == None:
		self.bld.fatal("No \"sce_sys\" folder found!")

	t_livearea = []
	sce_sys_src = sce_sys.ant_glob('**/*')
	sce_sys_out = self.path.get_bld().find_or_declare('sce_sys')
	for src in sce_sys_src:
		src_out = sce_sys_out.find_or_declare(src.path_from(sce_sys))
		if src.name.lower().endswith('.png'):
			t = self.create_task('ConvertPNGForLiveArea', [src], [src_out])
		else:
			t = self.create_task('CopyFile', [src], [src_out])
		t_livearea.append(t)

	t2 = self.create_task('create_param_sfo')
	t2.sce_sys = sce_sys
	t2.sce_sys_out = sce_sys_out
	t2.outputs = [sce_sys_out.find_or_declare('param.sfo')]
	t2.title_id = title_id
	t2.title_string = title_string

	t3 = self.create_task('create_vpk')
	t3.inputs = t1.outputs + t2.outputs + sce_sys_src
	t3.outputs = [self.path.get_bld().find_or_declare('%s.vpk'%self.target.split('.elf')[0])]

@TaskGen.feature("vita_shaders")
@TaskGen.before_method('process_romgen')
def tsk_vita_shark_shader(self):

	if self.bld.options.SHARKIP != None:
		ipsp = self.bld.options.SHARKIP.split(':')
		self.shark_ip = ipsp[0]
		self.shark_port = int(ipsp[1])

		#Create connect/disconnect tasks
		conn_task = self.create_task('EstablishConnection' , [], [])
		conn_task.host = self.shark_ip
		conn_task.vclaunch = False
		disconn_task = self.create_task('CloseConnection', [], [])
		disconn_task.conn = conn_task

		#create compile task
		shaders = self.to_nodes(getattr(self, 'shaders', []))
		shaders_out = [s.change_ext(".gxp") for s in shaders]
		compile_task = self.create_task('CompileShaderTask', shaders, shaders_out)
		compile_task.conn = conn_task

		self.romgen_source = getattr(self, 'romgen_source', [])
		self.romgen_source += shaders_out

		conn_task.dep_tasks = [compile_task]
		compile_task.set_run_after(conn_task)
		disconn_task.set_run_after(compile_task)
	elif getattr(self, 'shader_cache', None) != None:
		#no shark server, so skip compiling and use cache directory
		self.romgen_source = getattr(self, 'romgen_source', [])
		self.romgen_source += self.shader_cache.ant_glob("**/*.gxp")

def get_update_tasks(bld, assets = None):
	"""
		This will upload only the eboot.bin and assets that have
		changed since the last build

		Will write directly to application install folder based
		on the PROJECT_TITLEID
	"""
	out_tasks = []

	def mk_task(src, dst):
		t = UpdateFileTask(env=bld.env)
		t.set_inputs(src)
		t.dests = dst
		bld.add_to_group(t)
		out_tasks.append(t)

	#eboot.bin file
	mk_task(bld.path.find_or_declare("eboot.bin"), ["eboot.bin"])

	if assets != None:
		#asset files
		for ass in assets.ant_glob("**/*", excl="asset_proc.json", remove=False):
			dst = '%s/%s' % (assets.name, ass.path_from(assets))
			mk_task(ass, [dst])

	return out_tasks


def get_vpk_upload_tasks(bld, title_string):
	ip = bld.options.PSVITAIP
	if ip == None:
		bld.fatal('Must provide device address using --PSVITAIP option')
	
	srcname = "%s.vpk" % title_string

	print("Uploading \"%s\" to %s" % (srcname, ip))
	#TODO: switch to use EstablishConnection tasks?
	bld(
		rule = '${CURL} -T "${SRC}" ftp://%s/ux0:/data/'%ip,
		source = [srcname],
		always = True,
	)

	return []


def do_process_upload_tasks(bld, title_id, psvitaip, up_tasks):
	ip = psvitaip.split(':')[0]

	#Create connect/disconnect tasks
	conn_task = EstablishConnection(env=bld.env)
	conn_task.title_id = title_id
	conn_task.host = ip
	conn_task.vclaunch = bld.options.VCLAUNCH
	disconn_task = CloseConnection(env=bld.env)	
	disconn_task.conn = conn_task
	
	#Make sure upload task runs between conn/disconn
	for up_task in up_tasks:
		up_task.conn = conn_task
		up_task.set_run_after(conn_task)

	#Make sure disconn runs after upload tasks
	[disconn_task.set_run_after(uptask) for uptask in up_tasks]

	#Add manually-created conn/disconn tasks to current group
	bld.add_to_group(conn_task)
	bld.add_to_group(disconn_task)

@conf
def vita_update_app(bld, title_id, title_string, assets, psvitaip):
	up_tasks = get_update_tasks(bld, assets)
	do_process_upload_tasks(bld, title_id, psvitaip, up_tasks)
	
@conf
def vita_upload_vpk(bld, title_id, title_string, psvitaip):
	#this uses CURL directly, no need to create extra connections
	get_vpk_upload_tasks(bld, title_string)
	#up_tasks = get_vpk_upload_tasks(bld, title_string)
	#do_process_upload_tasks(bld, title_id, psvitaip, up_tasks)


class EstablishConnection(Task.Task):
	"""
		TODO: update runnable_status so that this only runs
		if dependent tasks need to run
	"""
	always_run = True

	def run(self):
		self.vita_host = self.host
		self.vc_port = getattr(self, 'vc_port', 1338)
		cmd_destroy = "destroy\n"

		#send destroy command (can't delete eboot.bin of running app!)
		if self.vclaunch:
			s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			s.connect((self.vita_host, self.vc_port))
			s.send(cmd_destroy.encode("utf-8"))
			s.close()
			time.sleep(1)

		self.ftp_lock = threading.Lock()
		with self.ftp_lock:
			self.ftp = FTP()
			self.ftp.connect(host=self.vita_host, port=getattr(self, 'ftp_port', 1337))
			self.ftp.login()

			if getattr(self, 'title_id', None) != None:
				self.approot = "ux0:app/%s/" % (self.title_id)
				print("FTP CWD: %s" % self.approot);
				self.ftp.cwd(self.approot)
			else:
				self.ftp.cwd("ux0:/data")

class CloseConnection(Task.Task):
	always_run = True
	def run(self):
		with self.conn.ftp_lock:
			self.conn.ftp.quit()

		#send launch command
		if self.conn.vclaunch:
			cmd_launch = "launch %s\n" % self.env.PROJECT_TITLEID
			s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
			s.connect((self.conn.vita_host, self.conn.vc_port))
			s.send(cmd_launch.encode("utf-8"))
			s.close()

def show_prog(label, block, progress):
	progress[0] += len(block)
	blen = 60
	num = progress[1]
	i = progress[0]
	st = num / blen
	pct = int((i / num) * blen)
	pstr = "["
	for b in range(0, pct):
		pstr += '|'
	for b in range(pct, blen - 1):
		pstr += "-"
	pstr += "] %d / %d" % (i, num)
	print("UPLOAD PROGRESS: %s" % label)
	print(pstr)

def do_upload_file(conn, ftp, src, dst, plabel, ensure_directories = True):
	with conn.ftp_lock:
		print("uploading \"%s\" to \"%s\"" % (src, dst))

		if ensure_directories:
			#make all directories
			sp = dst.split("/")
			if len(sp) > 1:
				ddir = ""
				for i in range(0, len(sp) - 1):
					ddir += "%s/" % sp[i]
					try:
						print("\tMKD %s" % ddir);
						ftp.mkd(ddir)
					finally:
						continue

		with open(src, "rb") as ff:
			blocksize = 8192
			progress = [0, path.getsize(src)]
			cmd = "STOR %s" % dst
			print("cmd: %s" % cmd)
			ftp.storbinary(cmd, ff, blocksize, lambda b: show_prog(plabel, b, progress))

def do_download_file(conn, ftp, src, dst, plabel):
	with conn.ftp_lock:
		print("Downloading \"%s\" to \"%s\"" % (src, dst))

		with open(dst, "wb") as ff:
			blocksize = 8192
			cmd = "RETR %s" % src
			print("cmd: %s" % cmd)
			ftp.retrbinary(cmd, ff.write, blocksize)


class UpdateFileTask(Task.Task):
	after = "EstablishConnection"
	before = "CloseConnection"
	always_run = False
	def run(self):
		ftp = self.conn.ftp

		#upload all input files
		files = self.inputs
		dests = self.dests
		for i in range(0, len(files)):
			src = files[i].abspath()
			dst = dests[i]
			plabel = "%s (%d / %d)" % (src, i, len(files))
			do_upload_file(self.conn, ftp, src, dst, plabel)

class CompileShaderTask(Task.Task):
	after = "EstablishConnection"
	before = "CloseConnection"
	always_run = False

	def run(self):
		ftp = self.conn.ftp
		shark_ip = self.generator.shark_ip
		shark_port = self.generator.shark_port

		#send clear command
		s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		s.connect((shark_ip, shark_port))
		s.sendall(bytes("<CLEAR>", "utf-8"))
		data = s.recv(1024)
		s.close()

		shark_root = getattr(self, 'shark_root', '/ux0:data/SharkServer')

		#Upload input shaders
		files = self.inputs
		dests = ['%s/input/%s' % (shark_root, s.name) for s in files]
		for i in range(0, len(files)):
			src = files[i].abspath()
			dst = dests[i]
			plabel = "%s (%d / %d)" % (src, i, len(files))
			do_upload_file(self.conn, ftp, src, dst, plabel, False)

		#Initiate build
		build_cmd = ' '.join([s.name for s in files])
		build_cmd += ' ' #server currently needs space as terminator
		s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		s.connect((shark_ip, shark_port))
		s.sendall(bytes(build_cmd, "utf-8"))
		data = s.recv(1024)
		results = data.decode("utf-8").split('\n')
		s.close()

		if len(results) != len(dests) + 1:
			print('dests:\t %s\nresults:\t %s' % (str(dests), str(results)))
			self.bld.fatal("VitaShaRK compilation failed!")
			return

		#Download artifacts
		for i in range(0, len(dests)):

			#################
			# TODO: maybe optimize this
			d_filename = dests[i].split('/')[-1:]
			found = -1
			for ri in range(0, len(results)):
				f_filename = results[ri].split('/')[-1:]
				if d_filename == f_filename:
					found = ri
					break
			assert(found != -1)
			#################

			plabel = "%s (%d / %d)" % (results[found], i, len(dests))
			do_download_file(self.conn, ftp, '/'+results[found], self.outputs[i].abspath(), plabel)

		#Copy outputs to shader_cache if applicable
		if getattr(self.generator, 'shader_cache', None) != None:
			for file in self.outputs:
				dst = '%s/%s' % (self.generator.shader_cache.abspath(), file.name)
				shutil.copyfile(file.abspath(), dst)
		else:
			waflib.Logs.warn("No shader_cache provided; Future builds will fail unless shark server is available.")

class create_eboot(Task.Task):
	after = ['cxxprogram']
	before = ['create_vpk']
	color  = 'BLUE'

	def run(self):
		gen = self.generator
		out_name = gen.target
		bldroot = gen.path.get_bld()

		if getattr(self.generator, 'strip', True):
			#Strip elf
			strip_out = gen.path.get_bld().find_or_declare('%s.stripped' % out_name)
			cmd = [gen.bld.env.VITA_STRIP[0], '-g', '-o', strip_out.abspath(), gen.link_task.outputs[0].abspath()]
			gen.bld.cmd_and_log(cmd, cwd=bldroot, quiet=Context.STDOUT)
		else:
			strip_out = gen.link_task.outputs[0]

		#Create velf
		velf_out = gen.path.get_bld().find_or_declare('%s.velf' % out_name)
		cmd = [gen.bld.env.VITA_ELF_CREATE[0], strip_out.abspath(), velf_out.abspath()]
		gen.bld.cmd_and_log(cmd, cwd=bldroot, quiet=Context.STDOUT)

		#Create eboot.bin
		eboot_out = gen.path.get_bld().find_or_declare('eboot.bin')
		cmd = [gen.bld.env.VITA_MAKE_FSELF[0], velf_out.abspath(), eboot_out.abspath()]
		gen.bld.cmd_and_log(cmd, cwd=bldroot, quiet=Context.STDOUT)

		gen.outputs = [eboot_out]

class create_param_sfo(Task.Task):
	before = ['create_vpk']
	color = 'BLUE'

	def run(self):
		gen = self.generator
		out_name = gen.target
		bldroot = gen.path.get_bld()

		#Create param.sfo
		param_out = self.sce_sys_out.find_or_declare('param.sfo')
		cmd = [
			gen.bld.env.VITA_MKSFOEX[0],
			'-s', 'TITLE_ID=%s' % self.title_id,
			'-d', 'ATTRIBUTE2=12', #gives us more ram!
			'%s' % self.title_string,
			param_out.abspath()
		]
		gen.bld.cmd_and_log(cmd, cwd=bldroot, quiet=Context.STDOUT)

class create_vpk(Task.Task):
	after = ['create_eboot', 'create_param_sfo', 'cxxprogram']
	color  = 'BLUE'
	def run(self):
		gen = self.generator
		out_name = gen.target.split('.elf')[0]
		bldroot = gen.path.get_bld()

		#this sucks
		asset_root = gen.bld.root.find_node(gen.bld.variant_dir)

		#Create VPK
		vpk_out = gen.path.get_bld().find_or_declare('%s.vpk' % out_name)
		ziproot = gen.path.get_bld()
		cmd = [
			gen.bld.env.ZIP[0], '-r',
			vpk_out.path_from(ziproot),
			gen.path.find_or_declare('eboot.bin').path_from(ziproot),
			gen.path.find_or_declare('sce_sys').path_from(ziproot)
		]
		gen.bld.cmd_and_log(cmd, cwd=bldroot, quiet=Context.STDOUT)
		
		if asset_root.find_node("assets") != None:
			#Append assets folder to VPK
			cmd = [
				gen.bld.env.ZIP[0],
				'-ur',
				vpk_out.path_from(asset_root),
				'assets'
			]
			gen.bld.cmd_and_log(cmd, cwd = asset_root)

		gen.outputs = [vpk_out]

class ConvertPNGForLiveArea(Task.Task):
	before = ['create_vpk']
	"""
		This task will convert a PNG file into a format that's
		compatible with the Vita livearea.

		This is particularly helpful because for some reason,
		PNGs exported from GIMP sometimes aren't accepted by the
		Vita even if the export options are correct.
	"""
	def run(self):
		if len(self.env.MAGICK_CONVERT) == 0:
			raise WafError("ImageMagick convert was not found during configuration")

		for i in range(0,len(self.inputs)):
			png = self.inputs[i]
			outpath = self.outputs[i]
			cmd = self.env.MAGICK_CONVERT + [
				png.abspath(),
				'+dither',
				'-colors', '128',
				#'-background', 'black',
				#'-alpha', 'background',
				'PNG8:%s' % outpath
			]
			self.exec_command(cmd)

class CopyFile(Task.Task):
	def run(self):
		for i in range(0,len(self.inputs)):
			self.exec_command(['cp', self.inputs[i].abspath(), self.outputs[i].abspath()])