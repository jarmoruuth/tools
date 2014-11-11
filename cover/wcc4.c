/**********************************************************************\
* C-preprocessor-postprocessor for Watcon wcc
\**********************************************************************/

#include <stdio.h>
#include <string.h>
#include <assert.h>


void postprocess(FILE *fp_in, FILE *fp_out);  
int get_char(FILE *fp_in); 
void unget_char(int c);

#define BUFSIZE 100

char 	buf[BUFSIZE];
int		bufpos = 0; 



void main(int argc, char **argv)
{
        FILE *fp_in, *fp_out;

        assert(argc > 2);

        fp_in = fopen(argv[1], "rt");
        fp_out = fopen(argv[2], "wt");

        assert(fp_in != NULL);
        assert(fp_out != NULL);
        postprocess(fp_in, fp_out);
}


/*
 * Removes comments (normal and single line) from the file and
 * fixes long lines (lines that are divided to multiple lines with '\')
 * to single line. Carriage returns are added when needed to keep
 * file's line numbers correct.
 * 
 */
void postprocess(FILE *fp_in, FILE *fp_out)
{


        int     c1,c2,c3,fixed,done,i,string;


        fixed = 0;
        string = 0;
        while ((c1 = get_char(fp_in)) != EOF) {  
        
            c2 = get_char(fp_in);
            c3 = get_char(fp_in);
            
            switch (c1) {
                
                case '\\':

                    //
                    //  case: \\n
                    //
                    if (c1 == '\\' && c2 == '\n') {
				    fixed++;
				    unget_char(c3);
            	    
                    //
                    //  case: \\\n  (in a string)
                    //
                    } else if (c1 == '\\' && c2 == '\\' && c3 == '\n' && string) {
            			putc(c1,fp_out);
            			unget_char(c3);
               			unget_char(c2);
            		//
                    //  case: \\ (in a string)
                    //
                    } else if (c1 == '\\' && c2 == '\\' && string) {
           				putc(c1,fp_out);
           				putc(c2,fp_out); 
           				unget_char(c3);
           		
                    //
                    //  case: \ (in a string)
                    //
                    } else if (c1 == '\\' && string) {
            			putc(c1,fp_out);
           				putc(c2,fp_out);
               			unget_char(c3);
           			//
                    //  case: \ 
                    //
                    } else if (c1 == '\\' && !string) {
             			putc(c1, fp_out);
           				unget_char(c3);
           				unget_char(c2);
            		} else { 
            			putc(c1, fp_out);
           				unget_char(c3);
           				unget_char(c2);
           			} 
                   break;
                    
                  
                case '\n':
                        
   	            if (c1 == '\n' && fixed > 0) {
            		    putc(c1,fp_out);
            			unget_char(c3);
               			unget_char(c2);
               			
                        for (i=0; i < fixed; i++) {
                            putc('\n', fp_out); 
                        }
                   	} else {  
                   		putc(c1,fp_out);
            			unget_char(c3);
              			unget_char(c2);
            		} 
            		fixed = 0;
            		break;
                   
                case '/':

                    //
                    // remove C-style comments
                    //
			    if (c1 == '/' && c2 == '*' && !string) {

				    done = 0;
					while (!done) {

				        if (c2 == '*' && c3 == '/') {
						    done = 1;
						} else if (c2 == '\n') {
							putc(c2, fp_out);
							unget_char(c3);
						} else {
							unget_char(c3);
						}
						if (!done) {
							c2 = get_char(fp_in);
							c3 = get_char(fp_in);
						}
					}
                    //
                    // remove C++-style comments
                    //
				} else if (c1 == '/' && c2 == '/' && !string) {
            				
            		    while (c3 != '\n') {
                            c3 = get_char(fp_in);
                        }
				    putc(c3, fp_out);

            		} else { 
            		    putc(c1, fp_out);
            	    	unget_char(c3);
                		unget_char(c2);
            		} 
                    break;
                        
                case '\'':

                    //
                    // case: '"' and '\"'
                    //
             	    if (c1 == '\'' && c2 == '"' && c3 == '\'' && !string ||
                        c1 == '\'' && c2 == '\\' && c3 == '"' && !string) {
                        putc(c1,fp_out);
                        putc(c2,fp_out);
                        putc(c3,fp_out);
                    } else {
                        putc(c1,fp_out);
                        unget_char(c3);
                        unget_char(c2);
                    }

                    break;

                case '"':

                    //
                    // toggle string mode on/off
                    //
                    if (string == 0) {
                        string = 1;
                    } else {
                        string = 0;
                    }
                    putc(c1, fp_out);
                    unget_char(c3);
                   	unget_char(c2);
                        
                    break;

                default:
                    putc(c1, fp_out);
                    unget_char(c3);
                	unget_char(c2);
                    break;
            }
        }
}

 
int get_char(FILE *fp_in)
{
	if (bufpos > 0) {
		return buf[--bufpos];
	} else {
		return getc(fp_in);
	}

}

void unget_char(int c)
{
	if (bufpos > BUFSIZE) {
		/* buffer overflow */
	} else {
		buf[bufpos++] = c;
	}

}
