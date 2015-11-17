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
#include <time.h>
#include <sys/time.h>
#include <openssl/md5.h>
#define MAX_MD5LENGTH 50
#define MAX_FILENAME 100
#define MAX_PENDING 5
#define MAX_LINE 256

int32_t file_exists(char * filename)
{
	FILE *file;
	int32_t size;
	if(file = fopen(filename, "r"))
	{
		fseek(file,0,SEEK_END);
		size = ftell(file);
		fseek(file,0,SEEK_SET);
		fclose(file);
		return size;
	}
	return -1;
}

void sendFile(int s, char *filename)
{
	char line[20000];
	FILE *fp = fopen(filename, "r");
	memset(line,'\0',sizeof(line));

	while(fread(line,sizeof(char),sizeof(line),fp))
	{
		printf("%s",line);
		send(s, line,sizeof(line),0);
		memset(line,'\0',sizeof(line));
	}
}

void requestFile(int s){
	char query[20];
	char filename[MAX_FILENAME];
	char md5server[MAX_MD5LENGTH];
	int filelen;
	int32_t filesize;
	float nBytes, start_time, end_time, throughput;
	struct timeval tv;
	// receive query from server
	memset(query,'\0',sizeof(query));
	while(strlen(query)==0){
		recv(s,query,sizeof(query),0);
	}
	if (strlen(query)==0) return;
	// get filename from user
	printf("%s ",query);
	scanf("%s",&filename);
	// send length of filename and filename
	filelen = strlen(filename);
	if (send(s,&filelen,sizeof(filelen),0)==-1){
		perror("client send error."); exit(1);
	}
	if (send(s,filename,sizeof(filename),0)==-1){
		perror("client send error."); exit(1);
	}
	// receive and decode file size
	filesize = 0;
	while (filesize==0){
		recv(s,&filesize,sizeof(int32_t),0);
	}
	if (filesize==-1){
		printf("File does not exist\n");
		return;
	}
	// receive md5 hash from server
	memset(md5server,'\0',sizeof(md5server));
	while (strlen(md5server)<=0){
		recv(s,md5server,sizeof(md5server),0);
	}
	// Calculate starting time
	gettimeofday(&tv,NULL);
	start_time = tv.tv_usec;
	// receive file from server
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
	// get end time and throughput
	gettimeofday(&tv,NULL);
	end_time = tv.tv_usec; //in microsecond
	float RTT = (end_time-start_time) * pow(10,-6); //RTT in seconds
	throughput = (nBytes*pow(10,-6))/RTT;
	// work out md5 hash of received file and compare them
	int size = filesize;
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
	// compare the md5 hashes
	if (strcmp(md5server,str)==0){
		printf("Transfer successful! Throughput: %f\n",throughput);
	} else {
		printf("Transfer unsuccessful.\n");
	}
}

void uploadFile(int s){
	// get and send filename to server
	char filename[MAX_FILENAME];
	int filelen;
	char ack[4];
	// ask user for file name
	printf("Please enter filename you would like to send: ");
	memset(filename,'\0',sizeof(filename));
	scanf("%s",filename);
	// get length of file name and send it to the user
	filelen = strlen(filename);
	filelen = htonl(filelen);
	if (send(s,&filelen,sizeof(filelen),0)==-1){
		perror("client send error."); exit(1);
	}
	// send file size to the user
	int32_t file_size = file_exists(filename);
	file_size = htonl(file_size);
	send(s,&file_size,sizeof(int32_t),0);
	// send filename to the user
	if (send(s,filename,sizeof(filename),0)==-1){
		perror("client send error."); exit(1);
	}
	// recieve ack from server and check if it is valid- if it is not, return
	memset(ack,'\0',sizeof(ack));
	while(strlen(ack)==0)
	{
		recv(s,ack,sizeof(ack),0);
	}
	if(strcmp(ack,"ACK") != 0)
	{
		printf("ACK not received");
		return;
	}
	sendFile(s,filename);

	//open file and get md5 hash
	int size = file_size;
	unsigned char md5[MD5_DIGEST_LENGTH];
	char* file_buffer = (char*) malloc(20000);
	int file_description;

	file_description = open(filename,O_RDONLY);
	file_buffer = mmap(0,size,PROT_READ,MAP_SHARED,file_description, 0);
	MD5((unsigned char*) file_buffer, size, md5);
	munmap(file_buffer, size);

	//turn md5hash into a string
	int i,j;
	char str[2*MD5_DIGEST_LENGTH+2];
	memset(str,'\0',sizeof(str));
	char str2[2];
	for(i=0; i<MD5_DIGEST_LENGTH; i++)
	{
		sprintf(str2,"%02x",md5[i]);
		str[i*2]=str2[0];
		str[(i*2)+1]=str2[1];
	}
	str[2*MD5_DIGEST_LENGTH]='\0';

	char md5str[strlen(str)+1];
	memcpy(md5str,str,strlen(str));
	md5str[strlen(str)] = '\0';
	send(s,md5str,strlen(str),0);

	char *result;

	while(strlen(result) == 0)
	{
		recv(s,result,sizeof(result),0);
	}

	if(!strcmp(result,"Unsuccessful transfer"))
	{
		printf("%s\n",result);
	}
	else
	{
		printf("%s\nSuccessful Transfer\n",result);
	}
}

void listDirectory(int s){

	int32_t size;
	int n = 0;
	float nBytes = 0;
	char file[100];
	size = 0;
	while(size == 0)
	{
		recv(s,&size,sizeof(size),0);
	}
	memset(file,'\0',sizeof(file));
	while(nBytes < size)
	{
		n = recv(s,file,sizeof(file),0);
		nBytes += n;
		printf("%s\n",file);
		fflush(stdout);
		memset(file,'\0',sizeof(file));
	}

}

void deleteFile(int s){
	char filename[MAX_FILENAME];
	int filelen;
	printf("Please enter filename you would like to send: ");
	scanf("%s",filename);
	filelen = strlen(filename);
	if (send(s,&filelen,sizeof(filelen),0)==-1){
		perror("client send error."); exit(1);
	}
	if (send(s,filename,sizeof(filename),0)==-1){
		perror("client send error."); exit(1);
	}
	
	char*file_exists_str;
	int file_exists = 0;

	while(file_exists == 0)
	{
		recv(s,&file_exists,sizeof(file_exists),0);
	}

	//file_exists = atoi(file_exists_str);

	if(file_exists == -1)
	{
		printf("File does not exist\n");
		return;
	}
	char response[5];
	printf("File Exists\nWould you like to delete the file? (Yes\\No)\n");
	scanf("%s",response);

	send(s,response,sizeof(response),0);

	int ack;

	while(ack==0)
	{
		recv(s,&ack,sizeof(ack),0);
	}
	if(ack == 1)
	{
		printf("File deleted successfully\n");
	}
	else
	{
		printf("File not deleted\n");
	}

}

int main(int argc, char * argv[])
{
	// declare variables
	FILE *fp;
	struct hostent *hp;
	struct sockaddr_in sin;
	char *host;
	int s, len, server_port;

	// Inputs: client host port filename
	if (argc==3) {
		host = argv[1];
		server_port = atoi(argv[2]);
	} else {
		fprintf(stderr, "usage: simplex-talk host\n");
		exit(1);
	}

	/* translate host name into peer's IP address */
	hp = gethostbyname(host);
	if (!hp) {
		fprintf(stderr, "simplex-talk: unknown host: %s\n", host);
		exit(1);
	}

	/* build address data structure */
	bzero((char *)&sin, sizeof(sin));
	sin.sin_family = AF_INET;
	bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
	sin.sin_port = htons(server_port);

	/* active open */
	if ((s = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		perror("simplex-talk: socket"); exit(1);
	}

	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		perror("simplex-talk: connect");
		close(s); exit(1);
	}

	while(1){
		char command[10];
		// prompt user to enter operation
		printf("Enter REQ, UPL, LIS, DEL or XIT: ");
		scanf("%s",&command);
		// send command to server
		if (send(s,command,sizeof(command),0)==-1){
			perror("client send error!"); exit(1);
		}

		// evaluate command
		if (strcmp(command,"REQ")==0){
			requestFile(s);
		} else if (strcmp(command,"UPL")==0){
			uploadFile(s);
		} else if (strcmp(command,"LIS")==0){
			listDirectory(s);
		} else if (strcmp(command,"DEL")==0){
			deleteFile(s);
		} else if (strcmp(command,"XIT")==0){
			close(s);
			exit(1);
		}
	}
}
