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
#include <openssl/md5.h>
#define SERVER_PORT 41017
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

char* getMD5hash(char * filename){
	// declare variables
	MD5_CTX mdContext;
	unsigned char result[MD5_DIGEST_LENGTH];

	// get md5 hash and store it in result
	FILE *file = fopen(filename,"rb");
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
			
	char *md5 = malloc(strlen(str)+1);
	memcpy(md5,str,strlen(str));
	md5[strlen(str)] = '\0';

	return md5;
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
	char file_len_str[10];
	//char md5[128]; //16 byte string?
	char *md5;

	//Get Query???
	char *query = "Get file name";
	//send query to client
	if(send(s,query,sizeof(query),0)==-1){
		perror("Server send error!"); exit(1);
	}
	//get length of file and declare file name
	memset(file_len_str,'\0',sizeof(file_len_str));
	while(strlen(file_len_str)==0){
		recv(s,file_len_str,sizeof(file_len_str),0);
	}
	file_len = atoi(file_len_str);
	char filename[file_len+1];
	//receive file name from client
	memset(filename,'\0',sizeof(filename));
	while(strlen(filename)==0){
		recv(s,filename,sizeof(filename),0);
	}
	//check if file exists and return file size. -1 if it doesn't.
	int32_t file_size = file_exists(filename);
	char file_size_s[100];
	sprintf(file_size_s,"%i",file_size);
	send(s,file_size_s,sizeof(file_size_s),0);
	if (file_size < 0){
		return;
	}
	//get md5 hash and send to client
	memset(md5,'\0',sizeof(md5));
	md5 = getMD5hash(filename);
	if(send(s, md5,16,0)==-1){
		perror("Server send error!"); exit(1);
	}
	//send file to client
	sendFile(s,filename);
}

void upload(int s){
	int filename_len;
	char file_len_str[10];
	char filesize[32];
	int32_t size;
	char md5client[100];
	char * md5;
	struct timeval tv;
	float start_time, end_time, nBytes, throughput;
	//get length of file and declare file name
	memset(file_len_str,'\0',sizeof(file_len_str));
	while(strlen(file_len_str)==0){
		recv(s,file_len_str,sizeof(file_len_str),0);
	}
	filename_len = atoi(file_len_str);
	char filename[filename_len+1];
	//receive file name from client
	memset(filename,'\0',sizeof(filename));
	while(strlen(filename)==0){
		recv(s,filename,sizeof(filename),0);
	}
	//send ACK
	char ack[4] = "ACK";
	if(send(s,ack,sizeof(ack),0)==-1){
		perror("Server send error!"); exit(1);
	}
	//receive size of file
	memset(filesize,'\0',sizeof(filesize));
	while(strlen(filesize)==0){
		recv(s,filesize,sizeof(filesize),0);
	}
	size = atoi(filesize);
	// Calculate starting time 
	gettimeofday(&tv,NULL);
	start_time = tv.tv_usec;
	//receive file from client and save to filename
	FILE *ofp = fopen(filename,"w");
	int n;
	char line[20000];
	memset(line,'\0',sizeof(line));
	while (nBytes < size) {
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
	//get md5 hash and send to client
	memset(md5,'\0',sizeof(md5));
	md5 = getMD5hash(filename);
	if(send(s, md5,16,0)==-1){
		perror("Server send error!"); exit(1);
	}
	//compare md5 hash values. If they are the same send throughput
	char *result;
	if (strcmp(md5client,md5)!=0){
		snprintf(result,100,"%f",throughput);
	} else {
		result = "Unsuccessful transfer";
	}
	send(s,result,sizeof(result),0);
}

void deleteFile(int s){
	int file_len;
	char file_len_str[10];
	int file_exists;
	char *status;
	//get length of file and declare file name
	memset(file_len_str,'\0',sizeof(file_len_str));
	while(strlen(file_len_str)==0){
		recv(s,file_len_str,sizeof(file_len_str),0);
	}
	file_len = atoi(file_len_str);
	char filename[file_len+1];
	//receive file name from client
	memset(filename,'\0',sizeof(filename));
	while(strlen(filename)==0){
		recv(s,filename,sizeof(filename),0);
	}
	//check if file exists
	if (access(filename,F_OK)!=-1){
		file_exists = 1;
	} else {
		file_exists = -1;
	}
	char *file_exists_str;
	sprintf(file_exists_str,"%f",file_exists);
	send(s,file_exists_str,sizeof(file_exists_str),0);
	//wait for confirmation from client
	while(strlen(status)==0){
		recv(s,status,sizeof(status),0);
	}
	//if the client confirmation is Yes, delete file
	if (strcmp(status,"Yes")){
		char *ack2;
		if (remove(filename)==1){
			ack2 = "deleted file successfully";
		} else {
			ack2 = "deleted file unsuccessfully";
		}
		send(s,ack2,sizeof(ack2),0);
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
			size += sizeof(file);
		}
		(void) closedir(dp);
		send(s,&size,sizeof(size),0);
		//sends over the directory
		dp = opendir("./");
		while (ep = readdir(dp)){
			file = ep->d_name;
			send(s,file,sizeof(file),0);
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

		if (strcmp(buf,"REQ")){
			clientRequest(new_s);
		} else if (strcmp(buf,"UPL")){
			upload(new_s);
		} else if (strcmp(buf,"DEL")){
			deleteFile(new_s);
		} else if (strcmp(buf,"LIS")){
			listDirectory(new_s);
		} else if (strcmp(buf,"XIT")){
			close(new_s);
		}
	}
}
