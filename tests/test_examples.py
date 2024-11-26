"""
This runs all examples and checks that no line starts with "ERROR:" (poor man's
testing, I know, but better than nothing).
It requires that examples have been built, as a native build.
"""
import os
from os.path import join, dirname, isfile
import platform
import subprocess

description = __doc__

#################################################

def makeParser():
	import argparse

	parser = argparse.ArgumentParser(
		prog="test_examples",
		description=description,
	)

	parser.add_argument(
		'-B', '--build',
		type=str, default=join(dirname(dirname(__file__)), "build"),
		help="Build directory where compiled examples are expected to live"
	)

	parser.add_argument(
		'-C', '--config',
		type=str, default="Debug",
		help="If multiconfig, specify which config to run"
	)

	return parser

#################################################

all_examples = [
	"00_no_codegen",
	"01_simple_kernel",
	"02_multiple_entrypoints",
	"03_module_import",
	"04_uniforms",
	"05_autodiff",
]

def main(args):
	is_multiconfig = isMulticonfigBuild(args.build)
	exe_ext = ".exe" if platform.system() == "Windows" else ""

	for example in all_examples:
		path = join(args.build, "examples", example)
		if is_multiconfig:
			path = join(path, args.config)
		exe_path = join(path, f"slang_webgpu_example_{example}{exe_ext}")
		runExample(example, exe_path)
	
#################################################

def runExample(example, exe_path):
	if not isfile(exe_path):
		print(f"Error: Could not find executable for example '{example}' (expected to be '{exe_path}')")
		exit(1)
	proc = subprocess.run(exe_path, capture_output=True)
	for i, line in enumerate(proc.stdout.decode().split("\n")):
		if line.startswith("ERROR:"):
			print(f"Example '{example}' failed at output line {i}:")
			print(line)
			print()
			print("Full log:")
			print(proc.stdout.decode())
			print(proc.stderr.decode())
			exit(1)
	print(f"Example '{example}' passed test.")

#################################################

def isMulticonfigBuild(build_dir):
	cache_path = join(build_dir, "CMakeCache.txt")
	if not isfile(cache_path):
		print("Error: You must first build examples, and provide the build directory using the '--build' argument.")
		exit(1)

	with open(cache_path) as f:
		for line in f:
			if line.startswith("#") or "=" not in line:
				continue
			key, value = line.split("=", 1)
			if key == "CMAKE_GENERATOR:INTERNAL":
				return isMulticonfigGenerator(value.strip())

	print("Warning: Could not find CMake Generator. Assuming monoconfig generator.")
	return False

#################################################

def isMulticonfigGenerator(cmake_generator):
	return cmake_generator.startswith("Visual Studio")

#################################################

if __name__ == '__main__':
	parser = makeParser()
	main(parser.parse_args())
