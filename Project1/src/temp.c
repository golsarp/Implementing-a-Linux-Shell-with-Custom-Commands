
// SARP GOL ID: 72368
//ASU TUTKU GOKCEK ID:71766

#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h> //termios, TCSANOW, ECHO, ICANON
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <dirent.h>
#include <regex.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
const char *sysname = "shellfyre";
void exe_command(char *name,char **args,int args_c);
void filesearch(char *search,int open,char *path);
void recursiveFileSearch(char *search ,int open, char *path);
void pstraverse(int PID, char *f);
int installed = 0;
enum return_codes
{
	SUCCESS = 0,
	EXIT = 1,
	UNKNOWN = 2,
};

struct command_t
{
	char *name;
	bool background;
	bool auto_complete;
	int arg_count;
	char **args;
	char *redirects[3];		// in/out redirection
	struct command_t *next; // for piping
};



/**
 * Prints a command struct
 * @param struct command_t *
 */
void print_command(struct command_t *command)
{
	int i = 0;
	printf("Command: <%s>\n", command->name);
	printf("\tIs Background: %s\n", command->background ? "yes" : "no");
	printf("\tNeeds Auto-complete: %s\n", command->auto_complete ? "yes" : "no");
	printf("\tRedirects:\n");
	for (i = 0; i < 3; i++)
		printf("\t\t%d: %s\n", i, command->redirects[i] ? command->redirects[i] : "N/A");
	printf("\tArguments (%d):\n", command->arg_count);
	for (i = 0; i < command->arg_count; ++i)
		printf("\t\tArg %d: %s\n", i, command->args[i]);
	if (command->next)
	{
		printf("\tPiped to:\n");
		print_command(command->next);
	}
}

/**
 * Release allocated memory of a command
 * @param  command [description]
 * @return         [description]
 */
int free_command(struct command_t *command)
{
	if (command->arg_count)
	{
		for (int i = 0; i < command->arg_count; ++i)
			free(command->args[i]);
		free(command->args);
	}
	for (int i = 0; i < 3; ++i)
		if (command->redirects[i])
			free(command->redirects[i]);
	if (command->next)
	{
		free_command(command->next);
		command->next = NULL;
	}
	free(command->name);
	free(command);
	return 0;
}

/**
 * Show the command prompt
 * @return [description]
 */
int show_prompt()
{
	char cwd[1024], hostname[1024];
	gethostname(hostname, sizeof(hostname));
	getcwd(cwd, sizeof(cwd));
	printf("%s@%s:%s %s$ ", getenv("USER"), hostname, cwd, sysname);
	return 0;
}

/**
 * Parse a command string into a command struct
 * @param  buf     [description]
 * @param  command [description]
 * @return         0
 */
int parse_command(char *buf, struct command_t *command)
{
	const char *splitters = " \t"; // split at whitespace
	int index, len;
	len = strlen(buf);
	while (len > 0 && strchr(splitters, buf[0]) != NULL) // trim left whitespace
	{
		buf++;
		len--;
	}
	while (len > 0 && strchr(splitters, buf[len - 1]) != NULL)
		buf[--len] = 0; // trim right whitespace

	if (len > 0 && buf[len - 1] == '?') // auto-complete
		command->auto_complete = true;
	if (len > 0 && buf[len - 1] == '&') // background
		command->background = true;

	char *pch = strtok(buf, splitters);
	command->name = (char *)malloc(strlen(pch) + 1);
	if (pch == NULL)
		command->name[0] = 0;
	else
		strcpy(command->name, pch);

	command->args = (char **)malloc(sizeof(char *));

	int redirect_index;
	int arg_index = 0;
	char temp_buf[1024], *arg;

	while (1)
	{
		// tokenize input on splitters
		pch = strtok(NULL, splitters);
		if (!pch)
			break;
		arg = temp_buf;
		strcpy(arg, pch);
		len = strlen(arg);

		if (len == 0)
			continue;										 // empty arg, go for next
		while (len > 0 && strchr(splitters, arg[0]) != NULL) // trim left whitespace
		{
			arg++;
			len--;
		}
		while (len > 0 && strchr(splitters, arg[len - 1]) != NULL)
			arg[--len] = 0; // trim right whitespace
		if (len == 0)
			continue; // empty arg, go for next

		// piping to another command
		if (strcmp(arg, "|") == 0)
		{
			struct command_t *c = malloc(sizeof(struct command_t));
			int l = strlen(pch);
			pch[l] = splitters[0]; // restore strtok termination
			index = 1;
			while (pch[index] == ' ' || pch[index] == '\t')
				index++; // skip whitespaces

			parse_command(pch + index, c);
			pch[l] = 0; // put back strtok termination
			command->next = c;
			continue;
		}

		// background process
		if (strcmp(arg, "&") == 0)
			continue; // handled before

		// handle input redirection
		redirect_index = -1;
		if (arg[0] == '<')
			redirect_index = 0;
		if (arg[0] == '>')
		{
			if (len > 1 && arg[1] == '>')
			{
				redirect_index = 2;
				arg++;
				len--;
			}
			else
				redirect_index = 1;
		}
		if (redirect_index != -1)
		{
			command->redirects[redirect_index] = malloc(len);
			strcpy(command->redirects[redirect_index], arg + 1);
			continue;
		}

		// normal arguments
		if (len > 2 && ((arg[0] == '"' && arg[len - 1] == '"') || (arg[0] == '\'' && arg[len - 1] == '\''))) // quote wrapped arg
		{
			arg[--len] = 0;
			arg++;
		}
		command->args = (char **)realloc(command->args, sizeof(char *) * (arg_index + 1));
		command->args[arg_index] = (char *)malloc(len + 1);
		strcpy(command->args[arg_index++], arg);
	}
	command->arg_count = arg_index;
	return 0;
}

void prompt_backspace()
{
	putchar(8);	  // go back 1
	putchar(' '); // write empty over
	putchar(8);	  // go back 1 again
}

/**
 * Prompt a command from the user
 * @param  buf      [description]
 * @param  buf_size [description]
 * @return          [description]
 */
int prompt(struct command_t *command)
{
	int index = 0;
	char c;
	char buf[4096];
	static char oldbuf[4096];

	// tcgetattr gets the parameters of the current terminal
	// STDIN_FILENO will tell tcgetattr that it should write the settings
	// of stdin to oldt
	static struct termios backup_termios, new_termios;
	tcgetattr(STDIN_FILENO, &backup_termios);
	new_termios = backup_termios;
	// ICANON normally takes care that one line at a time will be processed
	// that means it will return if it sees a "\n" or an EOF or an EOL
	new_termios.c_lflag &= ~(ICANON | ECHO); // Also disable automatic echo. We manually echo each char.
	// Those new settings will be set to STDIN
	// TCSANOW tells tcsetattr to change attributes immediately.
	tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

	// FIXME: backspace is applied before printing chars
	show_prompt();
	int multicode_state = 0;
	buf[0] = 0;

	while (1)
	{
		c = getchar();
		// printf("Keycode: %u\n", c); // DEBUG: uncomment for debugging

		if (c == 9) // handle tab
		{
			buf[index++] = '?'; // autocomplete
			break;
		}

		if (c == 127) // handle backspace
		{
			if (index > 0)
			{
				prompt_backspace();
				index--;
			}
			continue;
		}
		if (c == 27 && multicode_state == 0) // handle multi-code keys
		{
			multicode_state = 1;
			continue;
		}
		if (c == 91 && multicode_state == 1)
		{
			multicode_state = 2;
			continue;
		}
		if (c == 65 && multicode_state == 2) // up arrow
		{
			int i;
			while (index > 0)
			{
				prompt_backspace();
				index--;
			}
			for (i = 0; oldbuf[i]; ++i)
			{
				putchar(oldbuf[i]);
				buf[i] = oldbuf[i];
			}
			index = i;
			continue;
		}
		else
			multicode_state = 0;

		putchar(c); // echo the character
		buf[index++] = c;
		if (index >= sizeof(buf) - 1)
			break;
		if (c == '\n') // enter key
			break;
		if (c == 4) // Ctrl+D
			return EXIT;
	}
	if (index > 0 && buf[index - 1] == '\n') // trim newline from the end
		index--;
	buf[index++] = 0; // null terminate string

	strcpy(oldbuf, buf);

	parse_command(buf, command);

	// print_command(command); // DEBUG: uncomment for debugging

	// restore the old settings
	tcsetattr(STDIN_FILENO, TCSANOW, &backup_termios);
	return SUCCESS;
}

int process_command(struct command_t *command);

int main()
{
	while (1)
	{
		struct command_t *command = malloc(sizeof(struct command_t));
		memset(command, 0, sizeof(struct command_t)); // set all bytes to 0

		int code;
		code = prompt(command);
		if (code == EXIT)
			break;

		code = process_command(command);
		if (code == EXIT)
			break;

		free_command(command);
	}

	printf("\n");
	return 0;
}

int process_command(struct command_t *command)
{
	int r;
	if (strcmp(command->name, "") == 0)
		return SUCCESS;

	if (strcmp(command->name, "exit") == 0){
		if(installed == 1){
			
			pid_t pid = fork();
		if(pid == 0){
			
				//delete the module
				char *args[] = {"/usr/bin/sudo","rmmod","pstraverse.ko",0};
		 		printf("deleted module\n");
		 		execv(args[0],args);

		}
		
			
		}
			wait(NULL);
			return EXIT;
		
	}
		

	if (strcmp(command->name, "cd") == 0)
	{
		if (command->arg_count > 0)
		{
			r = chdir(command->args[0]);
			if (r == -1)
				printf("-%s: %s: %s\n", sysname, command->name, strerror(errno));
			return SUCCESS;
		}
	}

	// TODO: Implement your custom commands here
	if(strcmp(command->name,"filesearch") == 0){
		char cwd[300];
		getcwd(cwd,sizeof(cwd));
		if(command->arg_count > 1){
			//printf("i am\n ");
			//recursive and open
			if(command->arg_count > 2){
				
				recursiveFileSearch(command->args[2],1,cwd);
			}
			//open only
			 else if(strcmp(command->args[0],"-o") == 0 && (command->arg_count == 2)){
				//fileserach and open 
				printf("here");
				filesearch(command->args[1],1,cwd);
				
			}else if((strcmp(command->args[0],"-r") == 0) && (command->arg_count == 2)) {
				//recursive search
				//printf("recursive\n");
				recursiveFileSearch(command->args[1],0,cwd);
			}
		}else{
			//filesearch
			
			//printf("my arg: %s",command->args[0]);
			filesearch(command->args[0],0,cwd);
			//printf("end\n");
		}
	}
		if(strcmp(command->name,"pstraverse") == 0){
			//call pstraverse
			pstraverse(atoi(command->args[0]),command->args[1]);
		}
		
		//dna game
		//My awesome command for sarp
		if(strcmp(command->name,"dna") == 0){
			char nuc[] = "AGTTGCAG";
			int size = strlen(nuc);
			int t = 1;
			int mistake = 0;
			char a[] = "A";
			
			char cpy[size] ;
			char temp;
			//shuffle the string
			for(int i =0;i<size;i++){
				int r = rand() % size;
				cpy[i] = nuc[r];
			}
			printf("shuffled %s\n",cpy);
			char input[size + 10];
			

			
				

			
			printf("Enter your result:\n");
			//should be done in child process
			pid_t pid = fork();
			if(pid == 0){
				scanf("%s",input);
				if(strlen(input) != size){
				printf("You failed\n");
			}else{
				printf("input: %s\n",input);
				for(int i = 0;i<size;i++){
					//check the inputs

					if(cpy[i] == 'A' ){
						if(input[i] != 'T' ){
							printf("Wrong at index: %d\n",i);
							t = 0;
						}
					}
					if(cpy[i] == 'T' ){
						if(input[i] != 'A' ){
							printf("Wrong at index: %d\n",i);
							t = 0;
						}
					}

					if(cpy[i] == 'G' ){
						if(input[i] != 'C' ){
							printf("Wrong at index: %d\n",i);
							t = 0;
						}
					}
					if(cpy[i] == 'C' ){
						if(input[i] != 'G' ){
							printf("Wrong at index: %d\n",i);
							t = 0;
						}
					}
					
					
					
				}
				if(t == 1){
					printf("You passed nucleotide test\n");
					
				}else{
					printf("Come back after you study\n");
				}

				

				
			}
			exit(0);
			
			

			}else{
				wait(NULL);
			}
			
			



		}

		
		if(strcmp(command->name,"take") == 0){
			char *path = command->args[0];
			//printf("path: %s\n",path);
			int size = strlen(path);
			//get current directory
			char dir[40];
			getcwd(dir,sizeof(dir));
			//printf("directroy: %s\n",dir);
			DIR* check; 
			//input dir
			char *token = strtok(path,"/");
			while(token){
				//new dr
				strcat(dir,"/");
				strcat(dir,token);
				//printf("new path with comb: %s\n",dir);
				//check if directory exists
				check = opendir(dir);
				//if it does not exist create one 
				if(!check){
					mkdir(dir,0700);
				}
				//printf("after mkdir: %s\n",dir);
				//printf("token is: %s\n",token);

				token = strtok(NULL,"/");
			}
			//change to new directory
			chdir(dir);


		}

		if(strcmp(command->name,"joker") == 0){
			
			pid_t t = fork();
			if(t == 0){
				

				FILE *fp;
				//write job to file
			fp = fopen("job","w+");
			fputs("*/15 * * * * XDG_RUNTIME_DIR=/run/user/$(id -u) notify-send \"$(curl https://icanhazdadjoke.com/)\"\n",fp);
			
			fclose(fp);
			//execute scheduled job
			if (execl("/usr/bin/crontab","/usr/bin/crontab","job",NULL) == -1){
				printf("eroor\n");
			}	
					
				
			
			}else{
				wait(NULL);
			}
			
			

		}

		if(strcmp(command->name,"letters") == 0){
			//On terminal input of user may not be visible until enter is pressed.
			char input[50];
			char letter[1]="a";
			char temp[1];

			int num = 0;
			printf("Enter a word:\n");

           	 pid_t child = fork();

		    if(child == 0){
				scanf("%s", input);
			
				for(int i=0; i<strlen(input); i++){
					for(int j=0; j<strlen(input); j++){
						if(input[i]==input[j]){
							num++;
						}
					}
					temp[0]=input[i];
					printf("There are %d of letter %s .\n", num, temp);
					num=0;
				}

					
				exit(0);
			}else{
				wait(NULL);
          	}
		}

		if(strcmp(command->name,"cdh") == 0){
			printf("I ran out of time but I have ideas about how to implement it, they are written in code.\n");
			/*
			Implementation ides:
			In order to implement this part I thought about making a linked list or a 
			que structure. We must add the visited directories to the que whenever cd command or take command 
			is called since they are the commnands which switch directories.
			*/
		}

		



	pid_t pid = fork();

	if (pid == 0) // child
	{
		// increase args size by 2
		command->args = (char **)realloc(
			command->args, sizeof(char *) * (command->arg_count += 2));

		// shift everything forward by 1
		for (int i = command->arg_count - 2; i > 0; --i)
			command->args[i] = command->args[i - 1];

		// set args[0] as a copy of name
		command->args[0] = strdup(command->name);
		// set args[arg_count-1] (last) to NULL
		command->args[command->arg_count - 1] = NULL;

		/// TODO: do your own exec with path resolving using execv()
        char *com_name = command->name;
        char **com_args = command->args;
        int args_count = command->arg_count;
        
        exe_command(com_name,com_args,args_count);
        
		exit(0);
	}
	else
	{
		/// TODO: Wait for child to finish if command is not running in background
		//check command is in background
        if(!command->background){
			//wait for child to finish
            wait(0);
            
        }
		return SUCCESS;
		
	}

	printf("-%s: %s: command not found\n", sysname, command->name);
	return UNKNOWN;
}

 void exe_command(char *name,char **args,int args_c){
     //printf("name: %s",name);
     //printf("args: %s", args);
     //printf("number of args: %d",args_c);

     //basic command execution
     char name_c[50];
     strcpy(name_c,name);
     //printf("f called");
     char path[50] = "/bin/";
     strcat(path,name_c);
     execv(path,args);

 }

 void filesearch(char *search,int open, char *path){
	 regex_t regex;
	 int return_val;
	char s[50];
	strcpy(s,search);
	char p[50];
	strcpy(p,path);
	
	

	//printf("search %s ",s);
	 DIR *d;
  struct dirent *dir;
  d = opendir(".");
  if (d) {
    while ((dir = readdir(d)) != NULL) {
		
		//check findings using regex
		return_val = regcomp(&regex,s,0);
		return_val = regexec(&regex,dir->d_name,0,NULL,0);
		
			
		if(return_val == 0){
			//create file path
			char op[50];
			strcpy(op,path);
			strcat(op,"/");
			strcat(op,dir->d_name);
			printf("res :%s\n",op);


			


			//printf("%s\n", dir->d_name);
			char file[50];
			strcpy(file,dir->d_name);
			/*DEBUG
			//strcat(path,"/");
			//printf("s is: %s\n",s);
			//strcat(path,s);
			//strcat(path,file);
			//printf("path:%s\n",path);
			*/
			if(open == 1){
				
				pid_t p = fork();
				if(p == 0){
					execl("/usr/bin/xdg-open","xdg-open",op,(char *)0);
				}
				else{
					wait(NULL);
				}
			}
		}
		
      
    }
    closedir(d);
  }
 }

 void recursiveFileSearch(char *search ,int open, char *path){
	 regex_t regex;
	 int return_val;
	 char s[50];
	strcpy(s,search);
	//printf("serach : %s\n",s);
	 DIR *d = opendir(path);
  struct dirent *dir;
  //d = opendir(".");
  if(d == NULL) return;
    while ((dir = readdir(d)) != NULL) {
		
		if(dir-> d_type != DT_DIR){//not a directory
		//printf("dir %s\n",dir->d_name);
		//find matches using regex
		return_val = regcomp(&regex,s,0);
		return_val = regexec(&regex,dir->d_name,0,NULL,0);

		//check match
		if(return_val == 0){
			char res[50];
			strcpy(res,path);
			strcat(res,"/");
			strcat(res,dir->d_name);
			printf("res = %s\n",res);
			//check open parameters
			if(open == 1){
				
				
				pid_t p = fork();
				if(p == 0){
					//open the file 
					execl("/usr/bin/xdg-open","xdg-open",res,(char *)0);
				}
				else{
					wait(NULL);
				}
				
				
			}
		}
		
		}
		else
		if(dir -> d_type == DT_DIR && strcmp(dir->d_name,".")!=0 && strcmp(dir->d_name,"..")!=0 ) // if it is a directory
     	 {

			  //printf("%s\n", dir->d_name);
			  char new_path[258] = "";
			  
			  //printf("new path %s\n",path);
			  strcat(new_path,path);
			  strcat(new_path,"/");
			  strcat(new_path,dir->d_name);
			  /*DEBUG
			  //printf("new path: %s\n",new_path);
			 //sprintf(new_path, "%s/%s", path, dir->d_name);
			 //printf("new path : %s\n",new_path);
			 //recursiveFileSearch(s,open,new_path);
			 */
			 //call function recursively
			 recursiveFileSearch(s,open,new_path);
			 

		  }
		
	}
  
	closedir(d);

 }

 void pstraverse(int PID, char *f){
	 char root[10];
	char flag[10];
	sprintf(root, "pid=%d", PID);
	sprintf(flag, "flag=%s", f);
	//install the module
	 
	 char *args[] = {"/usr/bin/sudo","insmod","pstraverse.ko",root,flag,0};
	 /*
	 char *x[] = {"/usr/bin/sudo","rmmod","pstraverse.ko",0};
		 		printf("deleted module\n");
		 		execv(x[0],x);
				 */
		 //install if it is not installed
		 if(installed == 0){
			 installed = 1;
			 pid_t pid = fork();
		 if(pid == 0){
			
			 printf("installing module ... \n");
			 //install the module
			execv(args[0],args);
		 
		 }

		 	
		 } 
			 wait(NULL);
			 
			 
			 printf("install complete\n");

 }
