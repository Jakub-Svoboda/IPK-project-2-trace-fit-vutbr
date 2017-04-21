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

	int ttl = first_ttl; 		//set the desired first_ttl
	setsockopt(clientSocket, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
	int val=2;
	setsockopt(clientSocket, SOL_IP, SO_RCVTIMEO, &val, sizeof(val));

	//send the message
	if ((sendto(clientSocket, &packet, sizeof(packet) , 0 , (struct sockaddr *) &destinationAddress, slen)) <= 0){
		fprintf(stderr,"sendto() failed with error code %d\n",errno);
		exit(-1);
	}

	//receive
	val=1;
	/* Set the option, so we can receive errors */
	setsockopt(clientSocket, SOL_IP, IP_RECVERR,(char*)&val, sizeof(val));
	int return_status;
	struct iovec iov;                       /* Data array */
	struct msghdr msg;                      /* Message header */
	struct cmsghdr *cmsg;                   /* Control related data */
	struct sock_extended_err *sock_err;     /* Struct describing the error */ 
	
	while(1){
		//štruktúra pre adresu kompatibilná s IPv4 aj v6
		struct sockaddr_storage target; 
		char buf[1000];
		memset(buf,'\0', 1000);
		struct iovec iov; //io štruktúra
    
		struct msghdr msg; //prijatá správa - môže obsahovať viac control hlavičiek
		struct cmsghdr *cmsg; //konkrétna control hlavička

		struct icmphdr icmph; //ICMP hlavička
			
		iov.iov_base = &icmph; //budeme prijímať ICMP hlavičku
		iov.iov_len = sizeof(icmph); //dĺžka bude veľkosť ICMP hlavičky (obviously)

		msg.msg_name = &target; //tu sa uloží cieľ správy, teda adresa nášho stroja
		msg.msg_namelen = sizeof(target); //obvious
		msg.msg_iov = &iov; //opäť tá icmp hlavička
		msg.msg_iovlen = 1; //počet hlavičiek
		msg.msg_flags = 0; //žiadne flagy
		msg.msg_control = buf; //predpokladám že buffer pre control správy
		msg.msg_controllen = sizeof(buf);//obvious	
	
		/* Receiving errors flog is set */
		while(1){
			int res = recvmsg(clientSocket, &msg, MSG_ERRQUEUE); //prijme správu
			if (res<0) continue;
			
			/* lineárne viazaný zoznam - dá sa to napísať aj krajšie... */
			for (cmsg = CMSG_FIRSTHDR(&msg);  cmsg; cmsg =CMSG_NXTHDR(&msg, cmsg)) {
				/* skontrolujeme si pôvod správy - niečo podobné nám bude treba aj pre IPv6 */
				if (cmsg->cmsg_level == SOL_IP && cmsg->cmsg_type == IP_RECVERR){
					 //získame dáta z hlavičky
					 struct sock_extended_err *e = (struct sock_extended_err*) CMSG_DATA(cmsg);
					 //bude treba niečo podobné aj pre IPv6 (hint: iný flag)
					 if (e && e->ee_origin == SO_EE_ORIGIN_ICMP) {
						/* získame adresu - ak to robíte všeobecne tak sockaddr_storage */
						struct sockaddr_in *sin = (struct sockaddr_in *)(e+1); 						
						char str[4082];
						inet_ntop(AF_INET, &(sin->sin_addr), str, 4082);
						cout<<str<<endl;
						if(!strcmp(str, adress.c_str())){
							cout<<"target reached"<< endl;
						}
						 /*
						 * v sin máme zdrojovú adresu
						 * stačí ju už len vypísať viď: inet_ntop alebo getnameinfo
						 */

					//	 if (e->ee_type == ...)
					//	 {
							 /*
							 * Overíme si všetky možné návratové správy
							 * hlavne ICMP_TIME_EXCEEDED and ICMP_DEST_UNREACH
							 * v prvom prípade inkrementujeme TTL a pokračujeme
							 * v druhom prípade sme narazili na cieľ
							 * 
							 * kódy pre IPv4 nájdete tu
							 * http://man7.org/linux/man-pages/man7/icmp.7.html
							 * 
							 * kódy pre IPv6 sú ODLIŠNÉ!:
							 * nájdete ich napríklad tu https://tools.ietf.org/html/rfc4443
							 * strana 4
							 */
					//	 }
					}
				}          
			}  
						
			
		}
	
	}	

}