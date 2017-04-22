#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <pthread.h>
#include <linux/errqueue.h>
#include <chrono> 

#define PORTNUM 33434
using namespace std;
using namespace std::chrono ;

sockaddr_in getIpv4(struct hostent *server, string address, sockaddr_in * destinationAddress){
	bzero(destinationAddress, sizeof(* destinationAddress));		//null the server address
	uint32_t addressNum = inet_addr(address.c_str()); 
	if(addressNum == INADDR_NONE){
		bcopy((char *) server->h_addr, (char *)&destinationAddress->sin_addr.s_addr, server->h_length);
	}else{
		destinationAddress->sin_addr.s_addr=addressNum;
	}	
	destinationAddress->sin_family=AF_INET;
	destinationAddress->sin_port = htons(PORTNUM);
	return *destinationAddress;
}

void getIpv6(struct hostent *server, string address, sockaddr_in6 * destinationAddress6){
	bzero(destinationAddress6, sizeof(* destinationAddress6));		//null the server address
	inet_pton(AF_INET6, address.c_str(), &(destinationAddress6->sin6_addr));
	destinationAddress6->sin6_family=AF_INET6;
	destinationAddress6->sin6_flowinfo=0;
	destinationAddress6->sin6_port=htons(PORTNUM);
}

//Check whether the arguments are valid or not and assigns the values to apropriate variables.
void validateArgs(string *address, int argc, char* argv[],int * first_ttl, int * max_ttl){
	if(argc == 3 or argc==5){
		fprintf(stderr,"Error: Wrong number of arguments %d \n",argc);
		exit(1);
	}else if(argc ==2){			//address only
		*first_ttl=1;
		*max_ttl=30;
		*address = argv[1];
	}else if(argc == 4){		//address +1
		if(!strcmp(argv[1],"-f")){					//-f xx ip
			*first_ttl=atoi(argv[2]);
			*max_ttl=30;
			*address = argv[3];			
		}else if(!strcmp(argv[1],"-m")){			//-m xx ip
			*first_ttl=1;
			*max_ttl=atoi(argv[2]);
			*address = argv[3];
		}else{
			fprintf(stderr,"Parameter error.\n");
			exit(1);
		}		
	}else if(argc == 6){		//address +2
		if(!strcmp(argv[1],"-f") and !strcmp(argv[3],"-m")){			//-f xx -m yy ip
			*first_ttl=atoi(argv[2]);
			*max_ttl=atoi(argv[4]);
			*address = argv[5];	
		}else if(!strcmp(argv[1],"-m") and !strcmp(argv[3],"-f")){		//-m xx -f yy ip
			*first_ttl=atoi(argv[4]);
			*max_ttl=atoi(argv[2]);
			*address = argv[5];	
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
	string address;
	struct hostent *server = NULL;	
	validateArgs(&address,argc,argv, &first_ttl, &max_ttl);
	struct sockaddr_in destinationAddress; 
	struct sockaddr_in6 destinationAddress6;
	bool isIt6 = false;
	if(string::npos!=address.find('.')){				//ipv4
		destinationAddress=getIpv4(server, address, &destinationAddress);
	}else if(string::npos!=address.find(':')) {											//ipv6
		getIpv6(server,address, &destinationAddress6);
		isIt6=true;
	}else{
		cout<<"translation needed"<<endl;
		exit(1);
	}
		
	uint32_t clientSocket, socket6;
	//create a socket
	if(!isIt6){
		if ((clientSocket=socket(AF_INET, SOCK_DGRAM, 0)) <=0){
			fprintf(stderr,"Socket failed to create.\n");
			exit(EXIT_FAILURE);
		}
	}else{
		if ((socket6=socket(AF_INET6, SOCK_DGRAM, 0)) <=0){
			fprintf(stderr,"Socket failed to create.\n");
			exit(EXIT_FAILURE);
		}
	}	
	uint32_t slen=sizeof(destinationAddress);
	
	struct icmphdr packet;
	memset(&packet,0, sizeof(packet));
	packet.type = ICMP_ECHO;
	packet.code = 0;
	packet.un.echo.id = getpid();

	int val=2;
	setsockopt(clientSocket, SOL_IP, SO_RCVTIMEO, &val, sizeof(val));			//obsolete now

	//receive
	val=1;
	if(isIt6){
		setsockopt(socket6, SOL_IPV6, IPV6_RECVERR,(char*)&val, sizeof(val));		//turn on errors for ipv6
	}else{
		setsockopt(clientSocket, SOL_IP, IP_RECVERR,(char*)&val, sizeof(val));		//turn on errors for ipv4
	}

	struct sockaddr_storage target; 					//compatible with ipv4 and ipv6 
	char buf[1000];

	while(first_ttl<=max_ttl){
		memset(buf,'\0', 1000);							//null the receive buffer
		struct msghdr messageHeader; 					//reveived message
		struct cmsghdr *controlMessage; 				//control header
		struct icmphdr icmph; 							//received ICMP header
		
		struct iovec inputOutputVector; 				//iovec structure
		inputOutputVector.iov_base = &icmph; 			//set base to the reveived ICMP header
		inputOutputVector.iov_len = sizeof(icmph); 		

		messageHeader.msg_iov = &inputOutputVector; 	
		messageHeader.msg_iovlen = 1; 					//number of headers?
		messageHeader.msg_name = &target; 				
		messageHeader.msg_namelen = sizeof(target); 	
		messageHeader.msg_control = buf; 				
		messageHeader.msg_controllen = sizeof(buf);	

		if(isIt6){
			setsockopt(socket6, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &first_ttl, sizeof(first_ttl));
		}else{
			setsockopt(clientSocket, IPPROTO_IP, IP_TTL, &first_ttl, sizeof(first_ttl));
		}	
		
		//send the message
		cout<<"sending"<<endl;
		if(!isIt6){
			if ((sendto(clientSocket, &packet, sizeof(packet) , 0 , (struct sockaddr *) &destinationAddress, slen)) <= 0){
				fprintf(stderr,"sendto()4 failed with error code %d\n",errno);
				exit(-1);
			}
		}else{
			if ((sendto(socket6, &packet, sizeof(packet) , 0 , (struct sockaddr *) &destinationAddress6, sizeof(destinationAddress6))) <= 0){
				fprintf(stderr,"sendto()6 failed with error code %d\n",errno);
				exit(-1);
			}
		}
		cout<<"sent"<<first_ttl<<endl;
		auto timeStart = steady_clock::now();			//start time measurement
		while(1){															//cycles the recvmsg() until something arrives
			int res = -1;
			if(!isIt6){
				res = recvmsg(clientSocket, &messageHeader, MSG_ERRQUEUE); 	//reveive the message
			}else{
				res = recvmsg(socket6, &messageHeader, MSG_ERRQUEUE); 	//reveive the message
			}
			auto timeTmp = steady_clock::now();
			if((duration_cast<microseconds>(timeTmp-timeStart).count()) > 2000000){			//2 seconds timeout
				cout<<first_ttl<<"\t"<< "timeou reached" << "\t"<< "*" <<endl;
				break;
			}
			if (res<0) continue;
			cout<<res<<endl;
			auto timeEnd = steady_clock::now();
				
			for (controlMessage = CMSG_FIRSTHDR(&messageHeader);  controlMessage; controlMessage = CMSG_NXTHDR(&messageHeader, controlMessage)) {
				 struct sock_extended_err *error = (struct sock_extended_err*) CMSG_DATA(controlMessage);		//get the data from the header
				 if (error && error->ee_origin == SO_EE_ORIGIN_ICMP) {											//error must be ICMP type
					struct sockaddr_in * tmpAddress = (struct sockaddr_in *)(error+1); 							//get the address
					char str[4000];
					inet_ntop(AF_INET, &(tmpAddress->sin_addr), str, 4000);
					cout<<first_ttl<<"\t"<< str<< "\t"<< (duration_cast<microseconds>(timeEnd-timeStart).count()) / 1000.0<< " ms"<<endl;															//and print it
					if(!strcmp(str, address.c_str())){
						exit(0);						//the adress matches, program can exit
					}
				}          
			}
			break;										//breaks the recvmsg() cycle
		}
		first_ttl++;									//increments the ttl
	}	
}