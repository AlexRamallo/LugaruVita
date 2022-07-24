#!/usr/bin/env python3
from ftplib import FTP
import subprocess

"""
If using vitacompanion's FTP server, set to True
If using VitaShell's FTP server, set to False

Not tested with other servers
"""
use_vc = True

outdmp = "debug.psp2dmp"
outlog = "lugaru_runlog.txt"

logfile = "lugaru_runlog.txt"
elf = "build/LugaruVita.elf"

#logfile = "SharkServer.log"
#elf = "build/tool_vita_shark_eboot_debug/tool/shark_server/VitaShaRK Server.elf"

ftp = FTP()
ftp.connect(host='192.168.1.74', port=1337)
#ftp.connect(host='192.168.1.238', port=1337)
ftp.login()

months = {
	'Jan': 1,
	'Feb': 2,
	'Mar': 3,
	'Apr': 4,
	'May': 5,
	'Jun': 6,
	'Jul': 7,
	'Aug': 8,
	'Sep': 9,
	'Oct': 10,
	'Nov': 11,
	'Dec': 12
}

target_dump = None
dtv_h = 0

def cb(line):
	global dtv_h
	global target_dump

	if '.psp2dmp' in line:
		sp = line.split(' ')
		
		if use_vc:
			sp2 = sp[len(sp) - 2].split(':')
		else:
			sp2 = sp[8].split(':')
		
		dtv = (months[sp[5]] * 10000000)
		dtv += (int(sp[6]) * 10000) 
		dtv += (int(sp2[0]) * 100) 
		dtv += (int(sp2[1]))

		if dtv > dtv_h:
			target_dump = sp[len(sp) - 1]
			dtv_h = dtv

ftp.cwd('ux0:data/')
dir = ftp.retrlines('LIST', cb)

#Latest dump
with open(outdmp, 'wb') as fp:
	cmd = 'RETR %s' % target_dump
	print('cmd:\t"%s"' % cmd)
	res = ftp.retrbinary(cmd, fp.write)
	print(res)

#Latest log
got_log = False
with open(outlog, 'wb') as fp:
	try:
		cmd = 'RETR %s' % logfile
		print('cmd:\t"%s"' % cmd)
		res = ftp.retrbinary(cmd, fp.write)
		print(res)
		got_log = True
	except:
		print("Failed to get log file")

ftp.quit()

cmd = "python2 ../vita-parse-core/main.py \"%s\" %s" % (elf, outdmp)

if got_log:
	subprocess.call("konsole -e \"lnav %s\" &" % outlog, shell = True)

subprocess.call(cmd, shell=True)

