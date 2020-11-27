import shutil
import argparse
import os

parser = argparse.ArgumentParser(description='Create a base project for use with the Tanto library')
parser.add_argument('name', action="store")
args = parser.parse_args()
name = args.name

if os.path.isdir(name):
    r = input("Directory " + name + " already exists. Replace it? Y/n: ")
    if (r is 'Y'): shutil.rmtree(name)
    else: 
        print("Abort.")
        exit()

print("Creating project in " + name)

templatedir = os.path.join(os.getenv("HOME"), "dev", "hilt", "template")
shutil.copytree(templatedir, name, symlinks=True)

bindir  = os.path.join(name, "bin")
buildir = os.path.join(name, "build")
spvdir  = os.path.join(name, "shaders", "spv")

def rmAllFilesIn(dir):
    for item in os.listdir(dir):
        path = os.path.join(dir, item)
        os.unlink(path)

rmAllFilesIn(bindir)
rmAllFilesIn(buildir)
rmAllFilesIn(spvdir)

#replace occurances of template/TEMPLATE in file

def replaceTemplate(srcname, dstname, replacement):
    srcpath = os.path.join(name, srcname)
    dstpath = os.path.join(name, dstname)
    repcaps = str.upper(replacement)
    with open(srcpath, "rt") as src:
        with open(dstpath, "wt") as dst:
            for line in src:
                if "template" in line:
                    line = line.replace("template", replacement)
                if "TEMPLATE" in line:
                    line = line.replace("TEMPLATE", repcaps)
                dst.write(line)

def rename(old, new):
    os.rename(os.path.join(name, old), os.path.join(name, new))

rename("main.c", "oldmain.c")
rename("Makefile", "oldMakefile")

replaceTemplate("template.c", name + ".c", name)
replaceTemplate("template.h", name + ".h", name)
replaceTemplate("oldmain.c", "main.c", name)
replaceTemplate("oldMakefile", "Makefile", name)

def rm(filename):
    filepath = os.path.join(name, filename)
    os.unlink(filepath)

rm("template.c")
rm("template.h")
rm("oldmain.c")
rm("oldMakefile")
