# Framework of Visual Stimuli Discovery
Code of the framework of Visual Stimuli Discovery.

## Executables
There are multiple executables available in the framework.

### Master
Main executable of the framework. Following commandline arguments are available:
* `-v` (`--version`): Prints version of software
* `-d` (`--dataset`): Directory with log records
* `-s` (`--site`): Site to work on across participants (e.g., 'nih')
* `-t` (`--training`): Participant data to train classifier with' (e.g., 'p4')

### Trainer
Trainer executable of the framework. Following commandline arguments are available:
* `-d` (`--directory`): Directory where session is stored, without slash at the end of path.
* `-s` (`--session`): Session to be loaded, consisting of a .webm and a .json file
* `-m` (`--mode`): Mode of trainer. 'standard', 'label', 'feature_computation', 'store_view_masks', 'store_scroll_cache_map', and 'store_times' are available.
* `-p` (`--person`): Name of the person who labels. Reflected in file name.
* `--skip-perfect`): Skips observations without any BGR pixel value difference.

### Evaluator
Evaluator executable of the framework. Following commandline arguments are available:
* `-d` (`--visual-change-dataset`): Directory with log records (e.g., '/home/raphael/Dataset').
* `-i` (`--stimuli-dataset`): Directory with discovered stimuli to evaluate (e.g., '/home/stimuli').
* `-t` (`--task`): Filepath to image of the task (e.g., '/home/task.png').
* `-s` (`--site`): Site to work on across participants (e.g., 'nih').
* `-o` (`--output`): Directory for output (e.g., '/home/evaluator').
* `-e` (`--evaluation`): Identifier of evaluation (e.g., 'e1-webmd').
* `-v` (`--video-first`): First screencast and then stimuli are presented.

### Preciser
Preciser executable of the framework. Following commandline arguments are available:
* `-d` (`--visual-change-dataset`): Directory with log records (e.g., '/home/raphael/Dataset').
* `-i` (`--stimuli-dataset`): Directory with discovered stimuli to evaluate (e.g., '/home/stimuli').
* `-o` (`--evaluation-dataset`): Directory for output ('/home/evaluator').
* `-s` (`--site`): Site to work on across participants (e.g., 'nih').
* `-e` (`--evaluation`): Identifier of evaluation (e.g., 'e1-webmd').

### Finder
Finder executable of the framework. Following commandline arguments are available:
* `-d` (`--visual-change-dataset`): Directory with log records (e.g., '/home/raphael/Dataset').
* `-i` (`--stimuli-root-dataset`): Directory with discovered stimuli of root layer to evaluate (e.g., '/home/stimuli/0_html').
* `-s` (`--site`): Site to work on across participants (e.g., 'nih').
* `-o` (`--output`): Directory for output (e.g., '/home/finder_output').

### Reader
Reader executable of the framework. Following commandline arguments are available:
* `-d` (`--visual-change-dataset`): Directory with log records (e.g., '/home/raphael/Dataset').
* `-i` (`--stimuli-root-dataset`): Directory with discovered stimuli of root layer to evaluate (e.g., '/home/stimuli/0_html').
* `-o` (`--output`): Directory for output (e.g., '/home/finder_output').

### Video Walker (unmaintained)
Walks through the image frames of a WebM with VPX encoding. The current frame is displayed in a window.

### Log Explorer (unmaintained)
Explorer for log records. Following commandline arguments are available:
* `--video`: Path to the .webm of the log record
* `--data`: Path to the .json of the log record

## Notes
Spaces that are used in the framework:
- Viewport space: part of Web page displayed by browser
- Document space: Web page as described in DOM
Assumption: Screencast contains the complete viewport pixels (no desktop recording / partly visible viewport)

Terminology has been changed during the development process:
- IntraUserState == Stimulus Shot
- InterUserState == Visual Stimulus

## Third Party Content
In _res/tessdata_ you can find trained OCR models for the Tesseract library, taken from https://github.com/tesseract-ocr/tessdata
