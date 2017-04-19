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

#define PORTNUM 57588
using namespace std;

sockaddr_in getIpv4(struct hostent *server, string address, sockaddr_in * destinationAddress){
	bzero(destinationAddress, sizeof(destinationAddress));		//null the server address
	uint32_t addressNum = inet_addr(address.c_str()); 
	if(addressNum == INADDR_NONE){
		bcopy((char *) server->h_addr, (char *)&destinationAddress->sin_addr.s_addr, server->h_length);
	}else{
		destinationAddress->sin_addr.s_addr=addressNum;
	}	
	destinationAddress->sin_family=AF_INET;
	destinationAddress->sin_port = htons(PORTNUM);
	//bcopy(to_string(addressNum).c_str(), (char *)&destinationAddress->sin_addr.s_addr, strlen(to_string(addressNum).c_str()));
	return *destinationAddress;
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
	//struct sockaddr_in6 destinationAddress6;
	if(string::npos!=address.find('.')){				//ipv4
		destinationAddress=getIpv4(server, address, &destinationAddress);
	}else if(string::npos!=address.find(':')) {											//ipv6
		cout<<"ipv6"<<endl;
	}else{
		cout<<"translation needed"<<endl;
		exit(1);
	}
	uint32_t clientSocket;
	//create a socket
	if ((clientSocket=socket(AF_INET, SOCK_DGRAM, 0)) <=0){
        fprintf(stderr,"Socket failed to create.\n");
        exit(EXIT_FAILURE);
    }
		
	uint32_t slen=sizeof(destinationAddress);
	
	struct icmphdr packet;
	memset(&packet,0, sizeof(packet));
	packet.type = ICMP_ECHO;
	packet.code = 0;
	packet.un.echo.id = getpid();

	
	//packet.checksum=0;
	
	int ttl = 1; /* max = 255 */
	setsockopt(clientSocket, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
	int val=2;
	//setsockopt(clientSocket, SOL_IP, SO_RCVTIMEO, &val, sizeof(val));
	
	
	//send the message
	if ((sendto(clientSocket, &packet, sizeof(packet) , 0 , (struct sockaddr *) &destinationAddress, slen)) <= 0){
		fprintf(stderr,"sendto() failed with error code %d\n",errno);
		exit(-1);
	}

	//receive
	char buf[1000];
	memset(buf,'\0', 1000);

	val=2;
	//setsockopt(clientSocket, SOL_IP, SO_RCVTIMEO, &val, sizeof(val));
	val=1;
	/* Set the option, so we can receive errors */
	setsockopt(clientSocket, SOL_IP, IP_RECVERR,(char*)&val, sizeof(val));
	int return_status;
	struct iovec iov;                       /* Data array */
	struct msghdr msg;                      /* Message header */
	struct cmsghdr *cmsg;                   /* Control related data */
	struct sock_extended_err *sock_err;     /* Struct describing the error */ 
	
	while(1){
		iov.iov_base = &packet;
		iov.iov_len = sizeof(packet);
		msg.msg_name = (void*)&clientSocket;
		msg.msg_namelen = sizeof(clientSocket);
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_flags = 0;
		msg.msg_control = buf;
		msg.msg_controllen = sizeof(buf);
			
		/* Receiving errors flog is set */
		return_status = recvmsg(clientSocket, &msg, MSG_ERRQUEUE);
		if (return_status < 0) {
			//cout<<return_status<<endl;
			continue;
		}else{
			cout<<"got something"<<endl;
			//break;
		}	
		//cout<<"2BAR"<<endl;
		for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
			// Ip level 
			if (cmsg->cmsg_level == SOL_IP){
				// We received an error 
				if (cmsg->cmsg_type == IP_RECVERR){
					fprintf(stderr, "We got IP_RECVERR message\n");
					sock_err = (struct sock_extended_err*)CMSG_DATA(cmsg); 
					if (sock_err){
						// We are intrested in ICMP errors 
						if (sock_err->ee_origin == 2){
							// Handle ICMP errors types 
							switch (sock_err->ee_type){
								case ICMP_NET_UNREACH:
									// Hendle this error 
									fprintf(stderr, "Network Unreachable Error\n");
									break;
								case ICMP_HOST_UNREACH:
									// Hendle this error 
									fprintf(stderr, "Host Unreachable Error\n");
									break;
								// Handle all other cases. Find more errors :
								// http://lxr.linux.no/linux+v3.5/include/linux/icmp.h#L39
								//

							}
						}
					}
				}
			} 
		}
	}	
		
		
	
	
	
	
	
}