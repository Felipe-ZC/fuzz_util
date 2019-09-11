#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <pthread.h> 
#include <string.h>
#include <curl/curl.h>
#include <time.h>

/*
 * Read from stdin (wordlist), use this input to fuzz the url in
 * question!
 *
 * TODO: Accept input for url and wordlist file, and method.
 * NOTE: Remember to link the curl & pthreads libraries when compiling 
 * this file using gcc: 
 * gcc fuzz_url.c -lcurl -lpthread
 * */

void trim_nl(char *str)
{
	if(str[strlen(str)-1] == '\n')
		str[strlen(str)-1] = '\0';
}

typedef struct thread_args
{
	char *url;
	char *fname;
} args;

CURLcode fuzzy_request(char url[], char fuzz[], CURL *curl)
{
	
	//printf("URL is: %s\nfuzz is: %s", url, fuzz);

	//char fuzzy_url[strlen(url) + strlen(fuzz)];
	// Strings are null terminated so we add to the total length of url and fuzz...
	char *fuzzy_url = (char *)malloc(sizeof(char*) * sizeof(strlen(url) + strlen(fuzz) + 2));
	
  //printf("fuzzy_url1: %s\n", fuzzy_url);
	
	strcpy(fuzzy_url, url);
	strcat(fuzzy_url, fuzz);
	trim_nl(fuzzy_url);
	
  printf("Sending a GET request to: %s\n", fuzzy_url);

	// NOTE: Synch request...
	curl_easy_setopt(curl, CURLOPT_URL, fuzzy_url);
	CURLcode result = curl_easy_perform(curl);
	
	free(fuzzy_url);
	return result;
}

// Instead of writing to /dev/null use this function to ignore output...
size_t noop_cb(void *ptr, size_t size, size_t nmemb, void *data){ return size * nmemb; }

int write_buf(char *buf)
{	
	char out_file[BUFSIZ];
	sprintf(out_file, "out/%lu", (unsigned long)time(NULL));
	FILE *fp = fopen(out_file, "a+");
	fprintf(fp,"%s\n", buf);
	fclose(fp);
}

void *fuzzy_request_async(void *args)
{
	printf("in thread...");
	printf("%s", ((struct thread_args *)args)->url);
	printf("%s", ((struct thread_args *)args)->fname);

	char* base_url = ((struct thread_args *)args)->url;
	char * wl_name = ((struct thread_args *)args)->fname;
	FILE *fp = fopen(wl_name, "r");
	char buf[BUFSIZ]; // std name that gives a pretty big buffer
	CURL *curl;
    CURLcode res;
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, noop_cb);
	pthread_t new_thread;
	
	while(fgets(buf, sizeof buf, fp)) // read until end of file
   	{ 
		if(buf[strlen(buf)-1] == '\n')	   // check for buffer overflow...
		{ 
			res = fuzzy_request(((struct thread_args *)args)->url, buf, curl);
			if(res != CURLE_OK)
				fprintf(stderr, "fuzzy request failed: %s\n",
                curl_easy_strerror(res));
			else
		   	{
				long status;	
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
				printf("status: %ld\n", status);

				if(status < 400) 
				{
					char *new_url;	
				    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &new_url);
					printf("Possible match found using: %s", buf);
					printf("Forking a new process using %s as the base url...\n", new_url);
					write_buf(new_url);
					// TODO: Fork a new process (or thread) here,
					// using fuzzy_url as the new base_url 
				}	
			}
		}
	}

	fclose(fp);
	void *ptr = 0;
	return (ptr);
}

// TODO: Add option to read from wordlist file instead 
// of stdin...
//

// TODO: Multi-threading...
int main(int argc, char **argv) 
{
	
	int opt, file_flag, thread_flag, max_threads;
	char in_file[BUFSIZ];
	extern char* optarg;
	extern int optind;
	pthread_t thread_id;
	
	while((opt = getopt(argc, argv, "f:t:")) != -1) 
		switch(opt) {
			case 'f':
				file_flag = 1;
				strcpy(in_file, optarg);
				break;
			case 't':
				thread_flag = 1;
				max_threads = atoi(optarg);
				printf("tflag = %d", thread_flag);	
			  break;
			default:
				//TODO: Print help
				break;
		}
	

  // TODO: Needs error handling!	
	char *base_url = argv[optind];		
	args *t_args = (args*)malloc(sizeof(args));
	t_args->url = base_url;
	t_args->fname = (file_flag > 0) ? in_file : "stdin"; // Point to the same memory address
  
	printf("base_url is: %s\n", base_url);	
	printf("url is: %s\n", t_args->url);	
	printf("file name is: %s\n", t_args->fname);		
	
	//TODO: Loop here, create a new thread when a match is found...
	// main process...
		
	if(pthread_create(&thread_id, NULL, fuzzy_request_async, (void*)t_args) != 0){
		printf("Error!");

	} else pthread_join(thread_id, NULL);
	
	return 0;
}
