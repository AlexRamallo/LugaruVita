"""
	This is a replacement for CMake which gives more flexibility in vita workflow,
	and also includes a simple asset preprocessor for resizing and transcoding
	textures.

	Requires PVRTexTool and Python 'Pillow' library
"""
import os, shutil, math, json, waflib
from waflib import Configure, Build
from PIL import Image

use_packages = [
	"sdl2",
	"openal",
	"libpng",
	"libjpeg",
	"jsoncpp",
	"vorbis",
	"vorbisfile",
	"physfs",
]

platform_libs = [
	"vitashark",
	"z",
	"m",
	"mathneon",
	"SceGxm_stub",
	"SceDisplay_stub",
	"SceSysmodule_stub",
	"SceCommonDialog_stub",
	"SceLibKernel_stub",
	"SceKernelDmacMgr_stub",
	"SceKernelThreadMgr_stub",
	"SceCtrl_stub",
	"SceMotion_stub",
	"SceHid_stub",
	"SceTouch_stub",
	"SceShaccCg_stub",
	"SceShaccCgExt",
	"ScePvf_stub",
	"SceAppMgr_stub",
	"SceAppUtil_stub",
	"ScePgf_stub",
	"taihen_stub",
	"SceAudio_stub",
	"SceAudioIn_stub",
	"SceExcpmgrForKernel_stub"
]

#Subcontexts for release build so we have separate build folders
for base in [
	Configure.ConfigurationContext,
	Build.BuildContext,
	Build.CleanContext,
	Build.InstallContext,
	Build.UninstallContext]:
	class subcontext(base):
		cmd = "%s_release" % base.cmd.lower()
		variant = 'release'

def options(opt):
	opt.load("vitasdk compiler_cxx compiler_c")
	opt.load("color_gcc")
	opt.add_option('--no-vpk', dest="SKIP_VPK", action="store_true", help="Only build assets and eboot.bin, not VPK")
	opt.add_option('--PSVITAIP', dest='PSVITAIP', type='string')
	opt.add_option('--vclaunch', dest='VCLAUNCH', default=False, help='send launch command to vitacompanion after eboot update')
	opt.add_option('--profiling', dest='MICROPROFILE_ENABLED', action='store_true', default=False, help='enable profiling macros')
	opt.add_option('--no-audio', dest='AUDIO_DISABLED', action='store_true', default=False, help='disable audio')
	opt.add_option('--tex-quality', dest='TEXQUALITY', default='pvrtcfastest', help='default PVRTC encoding quality')
	opt.add_option('--no-pack', dest='SKIP_PACK', action='store_true', help='Don\'t package assets into a zip inside the VPK')
	opt.recurse("vitagl")

def opt_to_def(ctx, key):
	val = getattr(ctx.options, key, None)
	if val != None:
		if type(val) == bool:
			if val:
				val = '1'
			else:
				val = '0'
		else:
			val = str(val)
		ctx.env.append_unique('DEFINES', '%s=%s' % (key, val))

def configure(conf):
	conf.find_program("PVRTexToolCLI", var="PVRTT", mandatory=True)
	conf.load("color_gcc")
	conf.load("vitasdk", tooldir='.')
	conf.find_program('zip', var='ZIP', mandatory=True)

	conf.env.DESTDIR = "./INSTALL"
	conf.env.PROJECT_TITLEID = "LUGARU069"
	conf.env.PROJECT_NAME = "LugaruVita"
	
	conf.env.CFLAGS = [
		"-Wall",
		"-Wextra",
		"-Wno-parentheses",
		"-pedantic",
		"-ffast-math",
		"-mtune=cortex-a9",
		"-mfpu=neon",
	]
	
	conf.env.append_unique('CXXFLAGS', "--std=gnu++11")
	conf.env.append_unique('DEFINES', 'LOG_BUF=1');

	if not conf.options.SKIP_PACK:
		conf.env.append_unique('DEFINES', 'PACK_ASSETS=1')
	else:
		conf.env.append_unique('DEFINES', 'PACK_ASSETS=0')

	if 'release' in conf.cmd:
		conf.env.append_unique('CFLAGS', '-O0')
		#conf.env.append_unique('CFLAGS', '-g')
		#conf.env.append_unique('CFLAGS', '-flto')
		conf.env.append_unique('DEFINES', 'MICROPROFILE_WEBSERVER=0');
		conf.env.append_unique('DEFINES', 'NDEBUG=1');
	else:
		conf.env.append_unique('CXXFLAGS', '-fno-exceptions')
		conf.env.append_unique('CFLAGS', '-g')
		conf.env.append_unique('CFLAGS', '-O0')
		conf.env.append_unique('DEFINES', 'MICROPROFILE_WEBSERVER=0');

	opt_to_def(conf, 'MICROPROFILE_ENABLED')
	opt_to_def(conf, 'AUDIO_DISABLED')

	conf.env.CXXFLAGS.extend(conf.env.CFLAGS)
	conf.env.append_unique('CFLAGS', '-Wno-incompatible-pointer-types')

	#load packages
	for pkg in use_packages:
		conf.check_cfg(package=pkg, uselib_store=pkg, args=['--cflags', '--libs', '--static'])

	conf.recurse("vitagl")

def build(bld):
	#Data folder processing (see below)
	build_assets(bld)

	bld.add_group()

	bld.recurse("vitagl")

	defs = [
		"DATA_DIR=\"app0:Data\"",
    	"PLATFORM_VITA=1",
    	"PLATFORM_UNIX=1",
    	"BinIO_STDINT_HEADER=<stdint.h>",
	]

	if 'release' not in bld.variant:
		defs.extend(["DEBUG", "_DEBUG"])

	bld(
		rule = generate_version_header,
		source = "Source/Version.hpp.in",
		target = "Source/Version.hpp",
		major = 1,
		minor = 3,
		patch = 0,
		suffix = "-dev",
		release = "Vita Homebrew"
	)

	srcs = bld.path.ant_glob(["Source/**/*.cpp", "Source/**/*.c"])
	incs = bld.path.ant_glob(["Source/**"], dir=True, src=False)
	bld(
		source = srcs,
		includes = ["Source"] + incs,
		features = "c cxx",
		target = "Entrypoint",
		use = use_packages + ["vitaGL", "Source/Version.hpp"],
		defines = defs,
	)

	if bld.options.SKIP_VPK:
		variant = "vita_eboot"
	else:
		variant = "vita_vpk"

	bld(
		target=bld.env.PROJECT_NAME + '.elf',
		lib = platform_libs,
		use = ["Entrypoint"] + use_packages,
		linkflags = '-Wl,--emit-relocs',
		features='cxx cxxprogram %s' % variant,
		vita_title_id = bld.env.PROJECT_TITLEID,
		vita_title_string = bld.env.PROJECT_NAME,
		assets = bld.path.find_node("Data").get_bld().change_ext(".zip"),
		strip = 'release' in bld.variant
	)

	if bld.is_install > 0:
		ip = bld.options.PSVITAIP
		if ip == None:
			bld.fatal('Must provide device address using --PSVITAIP option')

		assets = bld.path.find_node("Data").get_bld()
		assets.mkdir()

		if not bld.options.SKIP_VPK:
			bld.vita_upload_vpk(bld.env.PROJECT_TITLEID, bld.env.PROJECT_NAME, ip)
		else:
			bld.vita_update_app(bld.env.PROJECT_TITLEID, bld.env.PROJECT_NAME, assets, ip)

asset_rules = {}

def load_rules(ctx, data_dir):
	rules_in = json.loads(data_dir.find_node("asset_proc.json").read())
	rules_out = {}
	missing = []
	for file, rules in rules_in.items():
		node = data_dir.find_node(file)
		if not node:
			missing.append(file)
			continue
		rules_out[node.abspath()] = rules
	if len(missing) > 0:
		ctx.fatal("The following files in asset_proc.json were not found in the Data folder:\n\t%s" % '\n\t'.join(missing))
		return None
	rules_out['__data_dir__'] = data_dir
	return rules_out

def get_rule(node, key = None, defval = None):
	global asset_rules
	try:
		rules = asset_rules[node.abspath()]
	except KeyError as e:
		return defval
	assert(rules != None)
	if key == None:
		return rules
	try:
		ret = rules[key]
	except KeyError:
		ret = defval
	finally:
		return ret

def build_assets(bld):
	data_dir = bld.path.find_node("Data")
	data_out = data_dir.get_bld()

	#glob patterns for image files
	img_glob = ['**/*%s' % e for e in ['.jpg', '.png']]

	#contains per-file overrides
	global asset_rules
	asset_rules = load_rules(bld, data_dir)
	rulesfile = data_dir.find_node("asset_proc.json")

	other = data_dir.ant_glob(incl='**/*', excl=img_glob + ["asset_proc.json"], dir = False)

	images = []
	for n in data_dir.ant_glob(incl=img_glob):
		if get_rule(n, "convert", True):
			images.append(n)
		else:
			other.append(n)

	#convert images
	for img in images:
		tg = bld(rule = do_encode_pvr, source = img, rename = False)
		#Ensure files are reprocessed if we modify the rules file
		tg.post()
		for task in tg.tasks:
			task.dep_nodes.append(rulesfile)
	
	#Copy other assets
	for ass in other:
		tg = bld(rule = do_copy, source = ass)
		#Ensure files are reprocessed if we modify the rules file
		tg.post()
		for task in tg.tasks:
			task.dep_nodes.append(rulesfile)

	#create asset package
	#warning: laziness below
	if not bld.env.SKIP_PACK:
		import threading
		datafiles = [n.get_bld() for n in images + other]
		ziplock = threading.Lock()
		def write_file_to_package(task):
			task.no_errcheck_out = True
			with ziplock:
				task.exec_command(f"{task.env.ZIP[0]} -0ur {task.generator.datazip} {task.inputs[0]}")
		#zip each file individually for fast partial rebuilds
		for file in datafiles:
			bld(
				rule = write_file_to_package,
				cwd = data_out.parent.abspath(),
				source = file,
				datazip = data_out
			)

def do_copy(task):
	for n in task.inputs:
		if get_rule(n, 'copy', True) == False:
			continue
		out = n.get_bld()
		out.parent.mkdir()
		shutil.copy(n.abspath(), out.abspath())

def do_encode_pvr(task):
	default_opts = {
		'format': 'PVRTCII_4BPP,UBN,sRGB',
		'quality': task.generator.bld.options.TEXQUALITY,
		'dither': False,
		'premultiply': False,
		'flip_x': False,
		'flip_y': True,
		'mipmap': True,
	}

	cmd = task.env.PVRTT
	for n in task.inputs:
		assert(get_rule(n, "convert", True))
		nopts = [
			'-f', get_rule(n, "format", default_opts["format"]),
			'-q', get_rule(n, "quality", default_opts["quality"])
		]
		if get_rule(n, "dither", default_opts["dither"]): nopts.append('-dither')
		if get_rule(n, "premultiply", default_opts["premultiply"]): nopts.append('-p')
		if get_rule(n, "flip_x", default_opts['flip_x']): nopts.extend(['-flip', 'x'])
		if get_rule(n, "flip_y", default_opts['flip_y']): nopts.extend(['-flip', 'y'])
		if get_rule(n, "mipmap", default_opts['mipmap']): nopts.append('-m')

		size = get_size(n)

		rsz = get_rule(n, "resize", None)
		if rsz != None:
			if rsz == False:
				out_size = None
			else:
				sp = rsz.split(',')
				assert(len(sp) == 2)
				w = int(sp[0])
				h = int(sp[1])
				out_size = (w, h)
		else:
			out_size = get_constrained_pot(size, 8, 512)

		if out_size != None:
			nopts.extend(['-r', '%d,%d' % out_size])

		if task.generator.rename:
			out = n.change_ext(".pvr")
		else:
			out = n.get_bld()
		out.parent.mkdir()
		task.outputs.append(out)
		task.exec_command(cmd + nopts + ['-i', n.abspath(), '-o', out.abspath()])

def get_size(node):
	with Image.open(node.abspath()) as img:
		return img.size

def get_constrained_pot(size, d_min, d_max):
	"""
		Returns the closest power-of-two dimensions for `size`, with
		each dimension bounded by [d_min, d_max] while attempting to
		respect the original aspect ratio to reduce distortion.
	"""
	def get_dt(d,r):
		dd = math.pow(2, d) * r
		if dd < d_min:
			return 1
		elif dd > d_max:
			return -1
		else:
			return 0
	(ww, hh) = size
	w = max(ww, hh)
	h = min(ww, hh)
	r = w / h
	w = round(math.log(w, 2))
	h = round(math.log(h, 2))
	for i in range(10):
		dt_w = get_dt(w,1)
		dt_h = get_dt(h,r)
		if dt_w == 0 and dt_h == 0:
			break
		else:
			w += dt_w
			h += dt_h
	if ww > hh:
		return (math.pow(2, w), math.pow(2, h))
	else:
		return (math.pow(2, h), math.pow(2, w))

def generate_version_header(task):
	insrc = task.inputs[0].read()

	major = str(getattr(task.generator, "major", "1"))
	minor = str(getattr(task.generator, "minor", "3"))
	patch = str(getattr(task.generator, "patch", "0"))
	suffix = str(getattr(task.generator, "suffix", ""))
	release_version = str(getattr(task.generator, "release", ""))
	vhash = "" #TODO

	if "_debug" in task.generator.bld.variant:
		build_type = "debug"
	else:
		build_type = "release"

	version = f"{major}.{minor}"

	if patch != "0":
		version += f".{patch}"

	verstr = f"{version}{suffix}"
	if vhash != "":
		verstr = f"{verstr} (git {vhash})"
	if release_version != "":
		verstr = f"{verstr} [{release_version}]"

	insrc = insrc.replace("@LUGARU_VERSION_MAJOR@", major)
	insrc = insrc.replace("@LUGARU_VERSION_MINOR@", minor)
	insrc = insrc.replace("@LUGARU_VERSION_PATCH@", patch)
	insrc = insrc.replace("@LUGARU_VERSION_NUMBER@", version)
	insrc = insrc.replace("@LUGARU_VERSION_SUFFIX@", suffix)
	insrc = insrc.replace("@LUGARU_VERSION_HASH@", vhash)
	insrc = insrc.replace("@LUGARU_VERSION_RELEASE@", release_version)
	insrc = insrc.replace("@LUGARU_VERSION_STRING@", verstr)
	insrc = insrc.replace("@CMAKE_BUILD_TYPE@", major)

	task.outputs[0].write(insrc)
	task.generator.export_includes = task.outputs
