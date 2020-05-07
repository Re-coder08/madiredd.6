1. Using random address generation
This algorithm is not very realistic, since programs tend to reuse the stored data.

2. Using weighted page address generation
The way algorithm is written,, it turns out this way.
  First we generate a random float value.
  Then we search for a higher value in the pages.
  When we find it, we use the page number and add a random offset.

  This all turns to a random page index, and random offset, which is the same as random address generation,
but in two steps.

To run the program:

1.make

2. ./oss

Output:

The output is stored in output.txt

3. version control - git
dir - /classes/OS/madireddd/madiredd.6

 
