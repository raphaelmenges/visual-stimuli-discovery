# Takes stimuli image-representations, gaze and mouse data as input; outputs overlaid images

import os
import csv
import re
import argparse
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.ticker import NullLocator

# Predefines
plots_folder = 'plots'
my_dpi = 96
fixation_vel_threshold = 50 # pixels
colors = {
	'p1': {'normal': '#d7191c', 'dark': '#600b0d', 'bright': '#ef7678'},
	'p2': {'normal': '#fdae61', 'dark': '#512901', 'bright': '#fec790'},
	'p3': {'normal': '#abd9e9', 'dark': '#46acce', 'bright': '#e3f2f8'},
	'p4': {'normal': '#2c7bb6', 'dark': '#1a486b', 'bright': '#7cb4df'}
}

# Parse command line arguments
parser = argparse.ArgumentParser()
parser.add_argument("-d", "--directory", help="directory with stimuli folder")
args = parser.parse_args()

if args.directory == None:
	print('Directory (absolute path) with stimuli folder must be defined')
	exit()

''' Function that takes raw gaze data and returns fixations '''
def detect_fixations(timestamps, gaze_x, gaze_y):

	# TODO: outlier correction, smoothing...

	fixations = ([],[],[]) # triple of durations (in ms), x values, and y values of fixations
	cur_fixation = (timestamps[0], [gaze_x[0]], [gaze_y[0]]) # start timestamp, gaze_x samples, and gaze_y samples
	for i in range(1,len(timestamps)):
		mean_x = np.mean(cur_fixation[1])
		mean_y = np.mean(cur_fixation[2])
		distance = np.sqrt((gaze_x[i] - mean_x)**2 + (gaze_y[i] - mean_y)**2)
		if distance > fixation_vel_threshold: # saccade faster than threshold, fixation finished
			fixations[0].append(timestamps[i] - cur_fixation[0]) # duration
			fixations[1].append(mean_x) # fixation_x
			fixations[2].append(mean_y) # fixation_y
			cur_fixation = (timestamps[i],[gaze_x[i]],[gaze_y[i]])
		else: # fixation does continue
			cur_fixation[1].append(gaze_x[i])
			cur_fixation[2].append(gaze_y[i])

	# Integrate last fixation
	fixations[0].append(timestamps[-1] - cur_fixation[0]) # duration
	fixations[1].append(np.mean(cur_fixation[1])) # fixation_x
	fixations[2].append(np.mean(cur_fixation[2])) # fixation_y

	# Return fixations
	return fixations

''' Function to plot data onto stimulus '''
def plot_on_stimulus(stimuli_dir, stimulus, mode):

	# Compose filepath
	filepath = stimuli_dir + "/" + stimulus

	# Create figure
	fig, ax = plt.subplots()
	ax.set_axis_off()

	# Load image
	img = plt.imread(filepath + '.png')
	img_w = img.shape[1]
	img_h = img.shape[0]

	# Check size of image
	if img_w > 65536 or img_h > 65536:
		print('Warning, image is too big to process! It is skipped.')
		return

	# Limit plot to the image extends
	ax.set_xlim([0,img_w])
	ax.set_ylim([img_h,0]) # y axis shall stay inverted

	# Put image into plot
	ax.imshow(img)

	# Plot gaze
	if mode == 'gaze' or mode == 'scanpath' or mode == 'scanpath_mouse':

		# Structure for gaze data: sessions -> shots -> gaze data
		gaze_data = {}

		# Load gaze data
		with open(filepath + '-gaze.csv') as csvfile:
			reader = csv.reader(csvfile, delimiter=',')
			next(reader, None) # skip the headers
			for row in reader:

				# Extract values from row
				session = row[0]
				shot_idx = int(row[1]) # aka intra_idx
				timestamp = int(row[2])
				x = int(row[3])
				y = int(row[4])

				# Put into dictionary for plotting
				if not session in gaze_data: # create session
					gaze_data[session] = {}
				if not shot_idx in gaze_data[session]: # create shot in session
					gaze_data[session][shot_idx] = []
				gaze_data[session][shot_idx].append((timestamp, x, y)) # append gaze data including timestamp

		# Go over sessions and shots covered by the stimulus
		for session,shots in gaze_data.items(): # for each session

			# Extract participant
			participant = session[:2]

			for shot,gaze in shots.items(): # for each shot

				# Raw gaze data
				(timestamps, gaze_x, gaze_y) = list(map(list, zip(*gaze)))

				# Plot raw gaze
				if mode == 'gaze':
					ax.scatter(
						gaze_x,
						gaze_y,
						c=colors[participant]['normal'],
						s=20,
						edgecolors=colors[participant]['bright'],
						linewidth=0.5)

				# Plot scanpath
				if mode == 'scanpath' or mode == 'scanpath_mouse':

					# Filter fixations
					(durations, fixations_x, fixations_y) = detect_fixations(timestamps, gaze_x, gaze_y)

					# Plot saccades between fixations
					for i in range(len(durations)-1):
						vec_x= fixations_x[i+1]-fixations_x[i]
						vec_y = fixations_y[i+1]-fixations_y[i]
						if not (vec_x == 0 and vec_y == 0):
							ax.arrow(
								fixations_x[i],
								fixations_y[i],
								vec_x,
								vec_y,
								fill=True,
								width=0.25,
								head_width=0,
								zorder=1,
								alpha=0.5,
								color='gray')

					# Plot fixations dots
					ax.scatter(
						fixations_x,
						fixations_y,
						s=8*durations,
						edgecolors=colors[participant]['normal'],
						linewidth=1,
						alpha=0.6,
						zorder=2,
						c=colors[participant]['normal'])

					# Plot fixation numbers
					for i in range(len(durations)):
						if durations[i] > 100: # ms threshold
							ax.annotate(
								str(i),
								xy=(fixations_x[i],fixations_y[i]),
								ha='center',
								va='center',
								zorder=3,
								color=colors[participant]['dark'],
								fontsize=10)

	# Plot mouse
	if mode == 'mouse' or mode == 'scanpath_mouse':

		# Mouse data
		mouse_data = {}

		# Load mouse data
		with open(filepath + '-mouse.csv') as csvfile:
			reader = csv.reader(csvfile, delimiter=',')
			next(reader, None) # skip the headers
			for row in reader:

				# Extract values from row
				session = row[0]
				shot_idx = int(row[1]) # aka intra_idx
				timestamp = int(row[2])
				x = int(row[3])
				y = int(row[4])
				event = row[5] # aka type (keyword already used by Python)

				# Put into dictionary for plotting
				if not session in mouse_data: # create session
					mouse_data[session] = {}
				if not shot_idx in mouse_data[session]: # create shot in session
					mouse_data[session][shot_idx] = []
				mouse_data[session][shot_idx].append((timestamp, x, y, event)) # append mouse data including timestamp and event type

		# Go over sessions and shots covered by the stimulus
		for session,shots in mouse_data.items(): # for each session

			# Extract participant
			participant = session[:2]

			for shot,mouse in shots.items(): # for each shot

				# Get separated lists
				(timestamps, mouse_x, mouse_y, event) = list(map(list, zip(*mouse)))

				# Plot mouse movements (also include clicks, mouse must move there as well)
				for i in range(len(timestamps)-1):
					vec_x = mouse_x[i+1]-mouse_x[i]
					vec_y = mouse_y[i+1]-mouse_y[i]
					if not (vec_x == 0 and vec_y == 0):
						ax.arrow(
							mouse_x[i],
							mouse_y[i],
							vec_x,
							vec_y,
							fill=True,
							width=0.25,
							head_width=0,
							zorder=5,
							alpha=0.75,
							color=colors[participant]['normal'])

				# Plot mouse clicks
				clicks_x = []
				clicks_y = []
				for i in range(len(timestamps)):
					if event[i] == 'click':
						clicks_x.append(mouse_x[i])
						clicks_y.append(mouse_y[i])

				# Plot a dot per click
				ax.scatter(
					clicks_x,
					clicks_y,
					s=60,
					edgecolors=colors[participant]['bright'],
					color=colors[participant]['dark'],
					linewidth=0.5,
					alpha=1,
					zorder=10)

	# Size figure
	fig.set_size_inches(img_w/my_dpi, img_h/my_dpi)

	# Attempt to remove all surrounding whitespace
	ax.set_frame_on(False)
	ax.margins(0,0)
	ax.xaxis.set_major_locator(NullLocator())
	ax.yaxis.set_major_locator(NullLocator())
	plt.tight_layout(pad=0)

	# Store figure
	try:
		file_fullpath = stimuli_dir + '/' + plots_folder + '/' + stimulus + '-' + mode + '.png'
		fig.savefig(
			file_fullpath,
			dpi=my_dpi*2,
			bbox_inches='tight',
			pad_inches=0)
	except:
		print("Could not store figure.") 
	plt.close(fig)

	# Message to user
	print('Stored:', file_fullpath)

''' Function to plot data onto simuli '''
def plot_on_stimuli(stimuli_dir):

	# Get all stimuli from the stimuli directory
	regex = re.compile('.png$')
	stimuli_files = [f for f in os.listdir(stimuli_dir) if os.path.isfile(os.path.join(stimuli_dir, f))]
	stimuli_files = list(filter(regex.search, stimuli_files))
	stimuli_files = [x[:-4] for x in stimuli_files]

	# Create directory for plots
	plots_folder_fullpath = stimuli_dir + '/' + plots_folder
	if not os.path.exists(plots_folder_fullpath):
		os.makedirs(plots_folder_fullpath)

	for stimulus in stimuli_files:
		# plot_on_stimulus(stimuli_dir, stimulus, 'gaze')
		# plot_on_stimulus(stimuli_dir, stimulus, 'scanpath')
		# plot_on_stimulus(stimuli_dir, stimulus, 'mouse')
		plot_on_stimulus(stimuli_dir, stimulus, 'scanpath_mouse')

# Call plotting for single layer
# plot_on_stimuli(r'/home/raphael/gm_out/2019-07-22_09-05-10/stimuli/0_html')

# Call plotting for all layer
for layer_dir in (next(os.walk(args.directory + '/stimuli'))[1]):
	plot_on_stimuli(args.directory + '/stimuli/' + layer_dir)