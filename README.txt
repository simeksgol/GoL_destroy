The purpuse of this program is to search for a way to add simple objects to a pattern in Conway's Game of Life, in a way that causes it to self destruct.

Prepare a LifeHistory pattern using Golly and save as an rle-file.

Use state 1 for still lifes to be destroyed and for the active pattern that initiates the destruction.
Use state 4 for the area where the program is allowed to place still lifes. All cells (in both phases for a blinker) must fit in the red area.
Use state 2 for the rest of the area that the active pattern is allowed to reach during the self destruct.

Run the program from the command line. There are two versions: Use destroy256.exe is you have a newer CPU (Inter Haswell or later), or use the somewhat slower destroy128.exe otherwise.

The command line format is:
destroy128 <pattern file> <objects> <max pool size> <max objects>

<objects> is a number of digits representing the type of objects the program may place:
1 = blocks, 2 = hives, 3 = blinkers, 4 = loaves, 5 = boats

<max pool size> describes how wide the search should be. The running time is proportional to this parameter.

<max objects> is the max number of objects the program will place. Currently the program will stop when the first solution is found, so this parameter can just be set to a high enough value.

For example:

> destroy128 demonoid.rle 124 5000 32

It happens sometimes that the program will fail to find one particular solution at one setting of <max pool size>, that it found at a lower setting of that parameter. This is a consequence of the search algorithm, and should not be considered a bug.
