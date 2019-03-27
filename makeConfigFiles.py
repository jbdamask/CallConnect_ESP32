import os, sys

cwd = os.path.split(os.getcwd())[1]


# Make the Config.h file
with open("Config.h.template", "r") as file:
	filedata = file.read()

filedata = filedata.replace('<device id>', cwd)
filedata = filedata.replace('<access point password>', sys.argv[1])
filedata = filedata.replace('<num pixels>', sys.argv[2])
filedata = filedata.replace('<endpoint from aws iot console>', sys.argv[3])
filedata = filedata.replace('<mqtt topic>', sys.argv[4])

with open("Config.h", "w") as file:
	file.write(filedata)

# Make the certificates.h file
rootdata = ""
certdata = ""
privdata = ""

def nl(f):
	nData = ""
	with open(f, "r") as data:
		for line in data:
			nData += (line.replace("\n", ("\\n\\" + "\n" if ("-END" not in line) else "\\n" ) )) if "\n" in line else (line + "\\n") 
	return nData


rootdata = nl("root.pem")
certdata = nl("cert.pem")
privdata = nl("privkey.pem")

with open("certificates.h.template", "r") as certificates:
	cdata = certificates.read()

cdata = cdata.replace('<root>', rootdata)
cdata = cdata.replace('<cert>', certdata)
cdata = cdata.replace('<priv>', privdata)


with open("certificates.h", "w") as wCert:
	wCert.write(cdata)