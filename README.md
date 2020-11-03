# SmallSH
SmallSH is a little sh-like interpreter developed for linux-based systems. It executes built-in and external programs. You are able to compile it for Windows, using g++ compiler (some linux sources are required or you may need to make some changes inside this program).
## Preparation
### 1. Clone
	git clone https://github.com/OlegYariga/SmallSH

### 2. Compile
    g++ main.cpp -o SmallSH

### 3. Run
    ./SmallSH

## Usage

### Built-in commands
	> cd
	> help
	> exit
	> lpwd
	> kill
	> declare

### Executing commands serially 
	> cd ..; pwd; ls -a; exit

### Coments
	> cd .. # You can type comment after #

### Symbols shielding
	> mkdir \#FOLDER\#; ls; cd /home/user/Рабочий\ стол; pwd  # Or you can shield some symbols

### Launching external program
	> gnome-calculator # Launch external program

### Launching program in background mode
	> gnome-calculator &; sensible-browser &
	[PID]15322
	[PID]15323

### I/O redirecting
	> echo 'You can redirect command output to file' >message.txt
	> cat<f
	Hub
	Middle
	Angr
	> sort<f
	Angr
	Middle
	Hub


