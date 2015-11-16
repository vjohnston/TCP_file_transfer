#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/time.h>
#include <openssl/md5.h>
#define MAX_MD5LENGTH 50
#define MAX_FILENAME 100
#define MAX_PENDING 5
#define MAX_LINE 256

int32_t file_exists(char * filename)
{
	FILE * file;
	int32_t size;
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

void sendFile(int s,char *filename){
	// open file and get buffer. Send buffer to client
	// send a line of 100 characters at a time (100 bytes)
	char line[20000];
	FILE *fp = fopen(filename, "r");
	memset(line,'\0',sizeof(line));

	while (fread(line,sizeof(char),sizeof(line),fp)){
		send(s, line,sizeof(line),0);
		memset(line,'\0',sizeof(line));
	}
}

void clientRequest(int s){
	int file_len;
	char query[20] = "Enter filename:";
	//char md5[16] = "hello there";
	//char *md5;
	//char md5[MAX_MD5LENGTH];
	//send query to client
	if(send(s,query,sizeof(query),0)==-1){
		perror("Server send error!"); exit(1);
	}
	//get length of file and declare file name
	while(file_len<=0){
		recv(s,&file_len,sizeof(file_len),0);
	}
	char filename[file_len+1];
	//receive file name from client
	memset(filename,'\0',sizeof(filename));
	while(strlen(filename)<file_len){
		recv(s,filename,sizeof(filename),0);
	}
	//check if file exists and return file size. -1 if it doesn't.
	int32_t file_size = file_exists(filename);
	send(s,&file_size,sizeof(int32_t),0);
	if (file_size < 0){
		return;
	}
	// open file and get md5 hash
	int size = file_size;
	unsigned char md5[MD5_DIGEST_LENGTH];
	char* file_buffer = (char*) malloc(20000);
	int file_description;

	file_description = open(filename,O_RDONLY);
	file_buffer = mmap(0,size, PROT_READ, MAP_SHARED, file_description, 0);
	MD5((unsigned char*) file_buffer, size, md5);
	munmap(file_buffer, size);

	// turn md5hash into a string
	int i,j;
	char str[2*MD5_DIGEST_LENGTH+2];
	memset(str,'\0',sizeof(str));
	char str2[2];
	for(i=0; i<MD5_DIGEST_LENGTH; i++) {
		sprintf(str2,"%02x",md5[i]);
		str[i*2]=str2[0];
		str[(i*2)+1]=str2[1];
	}
        str[2*MD5_DIGEST_LENGTH]='\0';

	char md5str[strlen(str)+1];
	memcpy(md5str,str,strlen(str));
	md5str[strlen(str)] = '\0';
	// send md5 hash
	send(s,md5str,strlen(md5str),0);
	//send file to client
	sendFile(s,filename);
}

void upload(int s){
	int filename_len = 0;
	char file_len_str[10];
	int32_t filesize;
	char md5client[100];
	struct timeval tv;
	float start_time, end_time, nBytes, throughput;
	//get length of file and declare file name
	memset(file_len_str,'\0',sizeof(file_len_str));
	while(filename_len<=0){
		recv(s,&filename_len,sizeof(filename_len),0);
	}
	//filename_len = atoi(file_len_str);
	printf("%i\n",filename_len);
	char filename[filename_len+1];
	//receive file name from client
	memset(filename,'\0',sizeof(filename));
	while(strlen(filename)==0){
		recv(s,filename,sizeof(filename),0);
	}

	printf("%s\n",filename);
	fflush(stdout);
	//send ACK
	char ack[4] = "ACK";
	if(send(s,ack,sizeof(ack),0)==-1){
		perror("Server send error!"); exit(1);
	}
	//receive size of file
	filesize = 0;
	printf("%i\n",filesize);
	while(filesize==0){
		recv(s,&filesize,sizeof(int32_t),0);
	}
	filesize = 9;
	printf("%i\n",filesize);
	//size = atoi(filesize);
	// Calculate starting time 
	gettimeofday(&tv,NULL);
	start_time = tv.tv_usec;
	//receive file from client and save to filename
	FILE *ofp = fopen(filename,"w");
	int n;
	char line[20000];
	memset(line,'\0',sizeof(line));
	while (nBytes < filesize) {
		n=recv(s,line,sizeof(line),0);
		nBytes += n;
		fwrite(line,sizeof(char),n,ofp);
		memset(line,'\0',sizeof(line));
	}
	// Calculate end time and throughput of file transfer
	gettimeofday(&tv,NULL);
	end_time = tv.tv_usec; //in microsecond
	float RTT = (end_time-start_time) * pow(10,-6); //RTT in seconds
	throughput = (nBytes*pow(10,-6))/RTT;
	//recieve md5 hash
	memset(md5client,'\0',sizeof(md5client));
	while(strlen(md5client)==0){
		recv(s,md5client,sizeof(md5client),0);
	}
	md5client[strlen(md5client)] = '\0';
	//get md5 hash
	int size = filesize;
	unsigned char md5[MD5_DIGEST_LENGTH];
	char* file_buffer = (char*) malloc(20000);
	int file_description;

	file_description = open(filename,O_RDONLY);
	file_buffer = mmap(0,size,PROT_READ,MAP_SHARED,file_description,0);
	MD5((unsigned char*) file_buffer, size, md5);
	munmap(file_buffer, size);

	int i,j;
	char str[2*MD5_DIGEST_LENGTH+2];
	memset(str,'\0',sizeof(str));
	char str2[2];
	for(i= 0; i<MD5_DIGEST_LENGTH; i++)
	{
		sprintf(str2,"%02x",md5[i]);
		str[i*2]=str2[0];
		str[(i*2)+1]=str2[1];
	}
	str[2*MD5_DIGEST_LENGTH]='\0';

	char md5str[strlen(str)+1];
	memcpy(md5str,str,strlen(str));
	md5str[strlen(str)] = '\0';

	//md5 = getMD5hash(filename);
	// GET MD5 HASH SAME WAY AS RECEIVe
	//compare md5 hash values. If they are the same send throughput
	//
	printf("%s\n",md5client);
	printf("%s\n",md5str);
	char *result;
	if (strcmp(md5client,md5str)==0){
		snprintf(result,100,"%f",throughput);
	} else {
		result = "Unsuccessful transfer";
	}
	send(s,result,sizeof(result),0);

	return;
}

void deleteFile(int s){
	int file_len = 0;
	char file_len_str[10];
	int file_exists;
	char *status;
	//get length of file and declare file name
	memset(file_len_str,'\0',sizeof(file_len_str));
	while(file_len <= 0){
		recv(s,&file_len,sizeof(file_len),0);
	}
	//file_len = atoi(file_len_str);
	char filename[file_len+1];
	//receive file name from client
	memset(filename,'\0',sizeof(filename));
	while(strlen(filename)==0){
		recv(s,filename,sizeof(filename),0);
	}
	printf("%s\n",filename);
	//check if file exists
	if (access(filename,F_OK)!=-1){
		file_exists = 1;
	} else {
		file_exists = -1;
	}
	printf("%i\n",file_exists);
	//sprintf(file_exists_str,"%f",file_exists);
	send(s,&file_exists,sizeof(file_exists),0);
	//wait for confirmation from client
	//
	while(strlen(status)==0){
		recv(s,status,sizeof(status),0);
	}
	//if the client confirmation is Yes, delete file
	if (strcmp(status,"Yes")){
		int ack2;
		if (remove(filename)==1){
			ack2 = 1;
		} else {
			ack2 = -1;
		}
		send(s,&ack2,sizeof(ack2),0);
	}
}

void listDirectory(int s){
	char *file, dir_size;
	int32_t size = 0;
	DIR *dp;
	struct dirent *ep;

	//open directory
	dp = opendir("./");
	if (dp!=NULL){
		//get directory size and send size to client
		while (ep = readdir(dp)){
			file = ep->d_name;
			printf("%s\n",file);
			size += sizeof(file);
		}
		printf("%i\n",size);
		(void) closedir(dp);
		send(s,&size,sizeof(size),0);
		//sends over the directory
		dp = opendir("./");
		while (ep = readdir(dp)){
			file = ep->d_name;
			//printf("%s\n",file);
			send(s,file,sizeof(file),0);
			memset(file,'\0',sizeof(file));
		}
	}
}

int main(int argc, char* argv[])
{
	struct sockaddr_in sin;
	int len;
	char buf[MAX_LINE];
	int s, new_s;
	int opt = 1;
	int server_port;

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

		if (strcmp(buf,"REQ")==0){
			clientRequest(new_s);
		} else if (strcmp(buf,"UPL")==0){
			upload(new_s);
		} else if (strcmp(buf,"DEL")==0){
			deleteFile(new_s);
		} else if (strcmp(buf,"LIS")==0){
			listDirectory(new_s);
		} else if (strcmp(buf,"XIT")==0){
			close(new_s);
		}
	}
}
