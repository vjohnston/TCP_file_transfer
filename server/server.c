#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <openssl/md5.h>
#define SERVER_PORT 41017
#define MAX_PENDING 5
#define MAX_LINE 256

int file_exists(char * filename)
{
	FILE * file;
	int size;
	if (file = fopen(filename, "r"))
	{
		fseek(file,0,SEEK_END);
		size = ftell(file);
		fseek(file,0,SEEK_SET);
		fclose(file);
		return size;
	}
	return -1;
}

int main(int argc, char* argv[])
{
	struct sockaddr_in sin;
	int len;
	char buf[MAX_LINE];
	int s, new_s;
	int opt = 1;
	int file_size;
	MD5_CTX mdContext;
	char file_size_s[100];
	int server_port;
	unsigned char result[MD5_DIGEST_LENGTH];

	if (argc==2)
	{
		server_port = atoi(argv[1]);
	} else {
		fprintf(stderr,"usage: simplex-talk host\n");
		exit(1);
	}


	/* build address data structure */
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_port = htons(server_port);

	/* setup passive open */
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("simplex-talk: socket");
		exit(1);
	}

	// set socket option
	if ((setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)& opt, sizeof(int)))<0){
		perror ("simplex-talk:setscokt");
		exit(1);
	}
	if ((bind(s, (struct sockaddr *)&sin, sizeof(sin))) < 0) {
		perror("simplex-talk: bind"); exit(1);
	}
	if ((listen(s, MAX_PENDING))<0){
		perror("simplex-talk: listen"); exit(1);
	} 

	/* wait for connection, then receive and print text */
	while(1) {
		if ((new_s = accept(s, (struct sockaddr *)&sin, &len)) < 0) {
			perror("simplex-talk: accept");
			exit(1);
		}


		if((len=recv(new_s, buf, sizeof(buf), 0))==-1){
			perror("Server Received Error!");
			exit(1);
		}
		if (len==0) break;

		file_size = file_exists(buf);
		sprintf(file_size_s,"%i",file_size);
		send(new_s,file_size_s,sizeof(file_size_s),0);
		
		if(file_size > 0)
		{
			// get md5 hash and store it in result
			FILE *file = fopen(buf,"rb");
			int bytes;
			unsigned char mdbuf[200000];
			MD5_Init(&mdContext);
			while((bytes = fread(mdbuf,sizeof(char),200000,file))!= 0){
				MD5_Update(&mdContext,mdbuf,bytes);
			}
			MD5_Final(result,&mdContext);
			
			// Put md5 hash in correct string format
			int i,j;
			char str[2*MD5_DIGEST_LENGTH+2];
			memset(str,'\0',sizeof(str));
			char str2[2];
			for(i=0; i<MD5_DIGEST_LENGTH; i++) {
				sprintf(str2,"%02x",result[i]);
				str[i*2]=str2[0];
				str[(i*2)+1]=str2[1];
			}
			str[2*MD5_DIGEST_LENGTH]='\0';
			
			char md5[strlen(str)+1];
			memcpy(md5,str,strlen(str));
			md5[strlen(str)] = '\0';
			
			// send MD5 to client
			if(send(new_s, md5,sizeof(md5),0)==-1){
				perror("Server send error!"); exit(1);
			}

			// open file and get buffer. Send buffer to client
			// send a line of 100 characters at a time (100 bytes)
			char line[20000];
			FILE *fp = fopen(buf, "r");
			memset(line,'\0',sizeof(line));
			
			while (fread(line,sizeof(char),sizeof(line),fp)){
				send(new_s, line,sizeof(line),0);
				memset(line,'\0',sizeof(line));
			}

		} else {
			perror("File does not exist\n");
			exit(1);
		}

		close(new_s);
	}
}
