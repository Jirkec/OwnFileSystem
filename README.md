## Key Features

- **Filesystem Simulation:**  
  Operates as a single file representing the entire filesystem, using i-nodes for file and directory management.

- **Custom File Structure:**  
  Includes a superblock, bitmaps for occupied i-nodes and clusters, pseudo-inode structures, and data blocks for files/directories/symlinks.

- **Command-line Interface:**  
  Supports essential file operations (copy, move, delete, create, list, read, change directory, etc.) via a set of Unix-like commands:  
  `cp`, `mv`, `rm`, `mkdir`, `rmdir`, `ls`, `cat`, `cd`, `pwd`, `info`, `incp`, `outcp`, `load`, `format`

- **User Manual & Documentation:**  
  Includes instructions for usage, file structure details, and command explanations.

- **Extensible Design:**  
  Divided into logical modules (`main.c`, `commands.h/.c`, `ext.h/.c`) for easy maintenance and enhancement.

---

## Usage

- Launch the program with the name of the pseudo-filesystem file as its only parameter.
- Format new or existing files to set up the filesystem structure.
- Perform typical file and directory operations using familiar commands.
- Advanced features include file import/export and batch command execution.

---

## Potential Improvements

- Full error handling and system robustness.
- Scaling the implementation for real-world use.
- Addition of encryption features for the bundle (e.g., VeraCrypt-like functionality).
