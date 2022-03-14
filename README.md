# SCAMP_TERM
A terminal for the SCAMP protocol. Audio input and output.

Daniel Marks,KW4TI proposed a new digital mode, named SCAMP: 

https://github.com/profdc9/RFBitBanger/tree/main/Docs

It's implementation easily fits in a microcontroller, it does not require time synchronization.
Because of this it nicely fits into the H7RTX radio (https://hackaday.io/project/180791-h7rtx)

For now, being the first man using this protocol, I find convenient to remotely receive my transmissions
on a web SDR. 
This terminal encodes and transmits, receive and decodes the FSK. Connect audio in and out to any audio channel and 
enjoy. PC's speakers and mike is enough for an audio loopback.
For my tests on the air the TX code is in the microcontroller, and decoding is performed by SCAMP_TERM connected to a Kiwi SDR.

Install:

Libraries required: libpthread, libportaudio, libncurses, libfftw3f, libm and their development packages, YMMV.
There is a Code Blocks project. Open it, compile and run.

Usage:

The screen is divided in three zones: the received characters are displayed at the top, The sync string
acts as a new line (\n)
Middle rows are for the number of bit errors that were corrected in each 2-characters string. (each string is 30 bit long and carries
2 characters, up to 3 bits can be corrected)
Lower part of the screen reads from the keyboard the TX message, Enter key starts transmission.

The code is configured for mark and space frequencies of 900 Hz and 1000 Hz. Bit timing is 40 millisecs. 
Can be altered via #defines in the .h files.

Warning:

The code is far from a production quality.
Many functions are not used and are there only for tests.

Performance:

40 m: tested OK from Italy to the most sensitive receivers in the east coast of USA, and Australia. 5 W and an out of tune dipole.

There is a loopback test that can be selected in TestSCAMP.h, it is possible to add gaussian noise.
In this condition reception is possible even when the noise is so high that audio tones can't be heard.

I know WSPR, FT8 and many other, better, protocols already exist. This comes out of my interest for weak signal communications, and a complete lack of interest
for ready to use devices.
