# Searches for frame, whether it is a blind one or not
# Call example: python .\BlindSearch.py -d "C:\StimuliDiscoveryData_prerelease\Dataset_stimuli\cnn\shots" -i 198 -p "p1"

import os
import csv
import re
import argparse

# Parse command line arguments
parser = argparse.ArgumentParser()
parser.add_argument('-d', '--directory', help='shots directory')
parser.add_argument('-i', '--frameidx', help='frame idx')
parser.add_argument('-p', '--participant', help='participant')
args = parser.parse_args()

if args.directory == None or args.frameidx == None or args.participant == None:
	print('Error. Provide parameters.')
	exit()

blind_files = []
for file in os.listdir(args.directory):
	if file.endswith('-blind.csv') and file.startswith(args.participant):
		blind_files.append(os.path.join(args.directory, file))

for file in blind_files:
	with open(file) as csvfile:
		reader = csv.reader(csvfile, delimiter=',')
		next(reader, None) # skip the headers
		for row in reader:
			if(int(args.frameidx) == int(row[0])):
				print('Blind! See at ' + file)