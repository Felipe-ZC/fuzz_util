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

char *parse_url(char *str) {
	if(str[strlen(str)-1] != '/')
		return strcat(str, "/");
	else return str;	
}

void trim_nl(char *str)
{
	if(str[strlen(str)-1] == '\n')
		str[strlen(str)-1] = '\0';
}

typedef struct thread_args
{
	char *url;
	char *fname;
} t_args;

CURLcode fuzzy_request(char url[], char fuzz[], CURL *curl)
{
	//printf("URL is: %s\nfuzz is: %s\n", url, fuzz);
	// Strings are null terminated so we add to the total length of url and fuzz...
	char *fuzzy_url = (char *)malloc(sizeof(char*) * BUFSIZ);
	
  //printf("fuzzy_url1: %s\n", fuzzy_url);
	
	strcpy(fuzzy_url, url);
	strcat(fuzzy_url, fuzz);
	trim_nl(fuzzy_url);
	parse_url(fuzzy_url);
	
  printf("Sending a GET request to: %s\n", fuzzy_url);

	// NOTE: Synch request...
	curl_easy_setopt(curl, CURLOPT_URL, fuzzy_url);
	
	// Follow redirects
 	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	CURLcode result = curl_easy_perform(curl);	
	free(fuzzy_url);

	return result;
}

// Instead of writing to /dev/null use this function to ignore output...
size_t noop_cb(void *ptr, size_t size, size_t nmemb, void *data){ return size * nmemb; }

int write_buf(char *buf)
{	
	char out_file[BUFSIZ];
	sprintf(out_file, "out/%lu.txt", (unsigned long)time(NULL));
	// printf("out file: %s", out_file);
	FILE *fp = fopen(out_file, "a+");
	fprintf(fp,"%s\n", buf);
	fclose(fp);
}

void *fuzzy_request_async(void *args)
{
/*printf("in thread...\n");
	printf("%s\n", ((struct thread_args *)args)->url);
	printf("%s\n", ((struct thread_args *)args)->fname);*/

	char base_url[BUFSIZ];
	char wl_name[BUFSIZ]; 
	char buf[BUFSIZ]; 
	FILE *fp;
	int pthread_result = 1;
	CURL *curl;
  CURLcode res;
	pthread_t new_thread;;
	t_args *new_args = (t_args*)malloc(sizeof(t_args));	
	new_args->url = (char*)malloc(sizeof(char*) * BUFSIZ);
	new_args->fname = (char*)malloc(sizeof(char*) * BUFSIZ);

	strcpy(base_url, ((t_args*)args)->url);
	strcpy(wl_name, ((t_args*)args)->fname);
	parse_url(base_url);

	fp = fopen(wl_name, "r"); // TODO: Check for null pointer...
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, noop_cb);
	
	while(fgets(buf, sizeof buf, fp)) // read until end of file
   	{ 
		if(buf[strlen(buf)-1] == '\n')	   // check for buffer overflow...
		{ 
			res = fuzzy_request(base_url, buf, curl);
			if(res != CURLE_OK)
				fprintf(stderr, "fuzzy request failed: %s\n",
                curl_easy_strerror(res));
			else
		   	{
				long status;	
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
				//printf("status: %ld\n", status);

				if(status < 400) 
				{
					char *new_url;	
				    
					curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &new_url);
				
					//printf("Possible match found using: %s", buf);
					
					//write_buf(new_url);
					printf("Sub 400 status code returned: %ld\n", status);
					// TODO: New url or base url... 
					/*sprintf(new_args->url, "%s/", new_url);	*/
					sprintf(new_args->url, "%s%s", base_url, buf);
					trim_nl(new_args->url);	
					printf("new url: %s\n", new_args->url);
					strcpy(new_args->fname, wl_name);	
					
					// TODO: Check that this url hasnt been used already...	
					// NOTE: Infinite loop here
					printf("Forking a new process using %s as the base url...\n", new_url);
					pthread_result = pthread_create(&new_thread, NULL, fuzzy_request_async, (void*)new_args);
					/*if(pthread_result == 0)	pthread_join(new_thread, NULL);*/
				}	else printf("status code >= 400: %ld\n", status);

			}
		}
	}
	
	printf("Done with %s\n", base_url);	
	
	free(new_args->url);		
	free(new_args->fname);		
	free(new_args);
	fclose(fp);
  curl_easy_cleanup(curl);

	if(pthread_result == 0) pthread_join(new_thread, NULL);
		
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
	char *wl_file = (file_flag > 0) ? in_file : "wls/default.txt"; // Point to the same memory address
	t_args *args = (t_args*)malloc(sizeof(t_args));
	args->url = (char*)malloc(sizeof(char*) * BUFSIZ);
	args->fname = (char*)malloc(sizeof(char*) * BUFSIZ);	
	
//	parse_url(base_url); // Add '/' character to the end of url, if its not there arleady...	
	strcpy(args->url, base_url);	
	strcpy(args->fname, wl_file);	

	/*printf("base_url is: %s\n", base_url);	*/
	printf("url is: %s\n", args->url);	
	printf("file name is: %s\n", args->fname);		
	
	if(pthread_create(&thread_id, NULL, fuzzy_request_async, (void*)args) != 0) {
		printf("Error!");
	} else pthread_join(thread_id, NULL);
	
	free(args->url);
	free(args->fname);
	free(args);

	return 0;
}
