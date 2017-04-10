#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <iostream>

using namespace std;

//Check whether the arguments are valid or not and assigns the values to apropriate variables.
void validateArgs(string *adress, int argc, char* argv[],int * first_ttl, int * max_ttl){
	if(argc == 3 or argc==5){
		fprintf(stderr,"Error: Wrong number of arguments %d \n",argc);
		exit(1);
	}else if(argc ==2){			//adress only
		*first_ttl=1;
		*max_ttl=30;
		*adress = argv[1];
	}else if(argc == 4){		//adress +1
		if(!strcmp(argv[1],"-f")){					//-f xx ip
			*first_ttl=atoi(argv[2]);
			*max_ttl=30;
			*adress = argv[3];			
		}else if(!strcmp(argv[1],"-m")){			//-m xx ip
			*first_ttl=1;
			*max_ttl=atoi(argv[2]);
			*adress = argv[3];
		}else{
			fprintf(stderr,"Parameter error.\n");
			exit(1);
		}		
	}else if(argc == 6){		//adress +2
		if(!strcmp(argv[1],"-f") and !strcmp(argv[3],"-m")){			//-f xx -m yy ip
			*first_ttl=atoi(argv[2]);
			*max_ttl=atoi(argv[4]);
			*adress = argv[5];	
		}else if(!strcmp(argv[1],"-m") and !strcmp(argv[3],"-f")){		//-m xx -f yy ip
			*first_ttl=atoi(argv[4]);
			*max_ttl=atoi(argv[2]);
			*adress = argv[5];	
		}else{
			fprintf(stderr,"Parameter error.\n");
			exit(1);
		}
	}else{
		fprintf(stderr,"Parameter error.\n");
		exit(1);
	}
}

int main(int argc, char* argv[]){
	if(argc <2 or argc>6){
		fprintf(stderr,"Error: Wrong number of arguments %d \n",argc);
		exit(1);		
	}
	int first_ttl, max_ttl;
	string adress;
	validateArgs(&adress,argc,argv, &first_ttl, &max_ttl);
	
	
}