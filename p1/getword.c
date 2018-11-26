/**
 * Chris Gilardi - 820489339 - cssc0101
 * CS 570 - Professor J. Carroll
 * Program One - Due 9/17/18
 **/

#include <stdio.h>
#include <stdlib.h>
#include "getword.h"

/*
* FILE: getword.c
* DESCRIPTION: The following function, getword(...) is a function designed to read characters from standard input and tokenize them into multiple words delimited by characters (described below).
*   The function reads each consecutive character from stdin (could be from a file, console, etc.), and will handle special characters (described below) in specific ways.
* INPUT: A character pointer *w, for storing each delimited word and special character needed in the calling function.
* OUTPUT: An int whose value, on return, is (generally):
* 	- 0 if the 'word' is a delimiter
*       - > 0 if the word is a valid set of characters creating a 'word'
* 	- -255 if the function recieves an EOF
* 	- < 0 if the word starts with a '$' (the length will be the word WITHOUT the $).
* SIDE EFFECTS: The character pointer (char* w) will be set to whichever word the function has found, followed by '\0', the null character (so the calling function knows where the string ends).
* SPECIAL CHARACTERS: ['\', '~', '$', '<', '|'] 
*  - These characters are treated specially, and in different ways. 
*    - '\' is the escape character, which will strip any special treatment off the next consecutive character
*    - '~' is a shorthand to insert the user's $HOME path variable, if at the start of a word
*    - '$' makes the returned length value of the following word negative
*    - '<' is treated specially, in that it will be treated as a word by itself, and will also be treated as a word in a pair (i.e. '<<')
*    - '|' is treated similarly to '<', but is not limited to pairs
* DELIMITERS: [' ' (space), '\n' (newline), EOF (end of file)]
*    - Delimiters are used to differentiate between words on the input stream. Spaces are ignored except in figuring out where a word should start/end, and newlines should be represented in the output as '\0' and have a length of 0
*/
int getword(char* w) {
	
	// nextchar holds the value of the character to check in the remainder of the function
	int nextchar;

	// index is used to calculate the length of the word and to ensure we do not run over the 255 character limit.
	int index = 0;
	
	// bool_isnegative is a "boolean" check for negative, and will only be set true if there is a $ as the first character of a word
	int bool_isnegative = 0;

	// bool_isescaped is a  "boolean" check for if the NEXT character should be escaped. It will always start at 0 because we have not yet found a '\' character.
	int bool_isescaped = 0;

	// bool_containednewline is used later in the function to see if a newline '\n' has been escaped by a backslash, to ensure the index stays correct.
	int bool_containednewline = 0;

	// This is where the main logic of the program happens. This loop checks each character as it is retrieved from stdin, and decides how to handle each various type.
	// To start, we set nextchar to the value of getchar() which pulls one character from stdin.
	while((nextchar = getchar())) {

		// We check for length before the rest of the checks, if we are already running over 254 characters, there is no need to do the rest of the checks, we will continue in the next call to getword().
		if(index >= 254) {
			// A NOTE ON UNGETC()
			// If, after going through characters, a word length is 254 characters, we must break the word in half to ensure it fits into our character buffer.
			// To do this, we use ungetc(). ungetc() simply places a character (in this case nexchar) back into an input stream (stdin). We use this function so that the next time through, getword() will be able to
			// continue from where it left off. This same procedure is used multiple times throughout this function to ensure no characters are missed in processing the input.
			ungetc(nextchar, stdin);
			
			// Break the loop to return what we have so far.
			break;
		}
		
		// Here we check for the presence of an escape character. If we have not already reached an escape character, bool_isescaped will be FALSE (0), and this block will run, setting bool_isescaped to TRUE (1),
		// which will allow this block to be skipped on the next run.
		// If we have already found an escape character, then we don't want this block to run, because we will just escape this character (as in '\\' -> '\')
		if(bool_isescaped == 0 && nextchar == '\\') {
			bool_isescaped = 1;
			continue;
		}
		
		// If we have already found an escape character, no special rules should be applied to whatever comes next, so we should simply skip the special character logic, since it's not needed.
		// The EXCEPTION to this rule is if we reach an EOF. Because this logic has already been implemented, we can simply throw the EOF through the else block, so that the program can handle it correctly.
                if(bool_isescaped != 0 && ( nextchar != EOF)) {
                        bool_isescaped = 0;			
                } else {

			// Check for spaces and tabs
 			// If there are any, break. This marks the end of the word
			// If it's the first character, just continue, since we have no word yet and we should ignore leading spaces
			if(nextchar == ' ') { 
				if(index == 0) {
					continue;
				}
				break;
			}

			// Handle the tilde '~' character.
			// If we are at the beginning of the word and have not already encountered a $ (perhaps this is redundant), we should replace a ~ with the user's $HOME environment variable			
			if(bool_isnegative == 0 && nextchar == '~' && index == 0) {
				
				// homedir holds the string (char*) representation of $HOME, so that we can use it later to concatenate into the word string.
				char *homedir = getenv("HOME");

				// Index used for inner loop
				int i;
	
				// This loop is used to concatenate the $HOME string, character-by-character, into the current *w pointer (to be used in the calling function)
				// On each pass, it will add a character onto this string, and increment index++, meaning the loop can continue exactly from the end of this loop on the next character, no other addition necessary.
				for(i = 0; homedir[i] !='\0'; i++) {
					w[index] = homedir[i];
					index++;
				}
				continue;
			}			
	
			// Check for ';', newline, and EOF
			// If we get one of these after a word has started, put it back so it can be parsed later [SEE 'A NOTE ON UNGETC()' above].
			// If they are first, simply set w to a null character.
			// On EOF, set the index to -255 for later.
			// TRICKY: The check for bool_isnegative. If it's negative (i.e. $ first), we must close the $ first (and return null) and THEN handle the ;, newline, or EOF.
			if(nextchar == ';' ||  nextchar == '\n' || nextchar == EOF) {
				if(index != 0 || bool_isnegative == 1){

					// Place the character back on the input stream (for an explanation of why this is necessary, please see 'A NOTE ON UNGETC()' above - in the length check [index >=254])
					ungetc(nextchar, stdin);
				} else {
					w[0] = '\0';
					if(nextchar == EOF) {
						index = -255;
					}
				}
				break;
	
			// If we've already encountered a $, set the returned index flag to be negative.
			} else if(nextchar == '$' && bool_isnegative != 1) {
				if(index == 0) {
					bool_isnegative = 1;
					continue;
				}
			// Check for the left carat '<' character. If we find one, it is treated as a delimiter and will be returned as a 1-length word. If we find 2, they will be tokenized as a pair '<<' length 2.
			} else if(nextchar == '<') {
				if(index != 0) {

					// We get here if the '<' is somewhere in the middle of a word.
					// Place the character back on the input stream (for an explanation of why this is necessary, please see 'A NOTE ON UNGETC()' above - in the length check [index >=254])
					ungetc(nextchar, stdin);
				} else {
					w[0] = '<';
					// We find the next character to see if it's also a '<'
					nextchar = getchar();
					index++;
					// If the next character is also a carat, we want to keep it and return both as a word length 2, else we should put the character back.
					if(nextchar == '<') {
						w[1] = '<';
						index++;
					} else {
						// If the second character is not another carat, we'll end up here and we need to come back to it since it's either a delimiter or the next word.
						// Place the character back on the input stream (for an explanation of why this is necessary, please see 'A NOTE ON UNGETC()' above - in the length check [index >=254])
						ungetc(nextchar, stdin);
					}
				}
				break;
			// Handle '|'
			// Similar to spaces, newlines, etc, if we find this character we should return it as a word of length 1. The difference is that wee must also return the character, not simply a '\0'
			} else if(nextchar == '|') {
				if(index != 0) {
                                        // Place the character back on the input stream (for an explanation of why this is necessary, please see 'A NOTE ON UNGETC()' above - in the length check [index >=254])
					ungetc(nextchar, stdin);
				} else {
					w[0] = '|';
					index++;
				}
				break;
			}
		}

		// This block removes newline characters since we don't want them to show up in the output of the calling function.
		if(nextchar == '\n') {
			// We set this flag to true so that we can handle and remove the \n after the loop.
			bool_containednewline = 1;
			nextchar = '\0';
		}
                w[index] = nextchar;
                index++;
	}
	// Null-terminate to ensure we don't get garbage in the output
	w[index] = '\0';	

	// Turn the returned index negative if we've encountered a '$' before the word.
	if(bool_isnegative == 1) {
		
		// Remove the first '$' from the output.
		w++;
		
		// Make it negative
		index = (index * -1);
	}

	// Even though the '\n' will have been replaced, the index will not update to reflect it, so if we have detected a newline somewhere in the word, we need to decrease the index to reflect it.
	if(bool_containednewline) {
		index--;
	}

	return index;
}
