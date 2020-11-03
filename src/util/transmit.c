#define  _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>
#include <zlib.h>
#include "transmit.h"
#include "zstring.h"
#include "base64.h"
#include "xalt_config.h"
#include "xalt_dir.h"
#include "xalt_c_utils.h"
#include "xalt_base_types.h"

const int syslog_msg_sz = SYSLOG_MSG_SZ;

void transmit(const char* transmission, const char* jsonStr, const char* kind, const char* key,
              const char* syshost, char* resultDir, const char* resultFn, FILE* my_stderr)
{
  char * cmdline = NULL;
  char * logNm   = NULL;
  char * p_dbg        = getenv("XALT_TRACING");
  int    xalt_tracing = (p_dbg && (strncmp(p_dbg,"yes",3)  == 0 ||
				   strncmp(p_dbg,"run",3)  == 0 ));

  if (strcasecmp(transmission,"directdb") == 0)
    {
      DEBUG0(my_stderr,"  Direct to DB transmission is NOT supported!\n");
      return;
    }


  if ((strcasecmp(transmission,"file")      != 0 ) &&
      (strcasecmp(transmission,"syslog")    != 0 ) && 
      (strcasecmp(transmission,"logger")    != 0 ) && 
      (strcasecmp(transmission,"none")      != 0 ) && 
      (strcasecmp(transmission,"syslogv1")  != 0 ) &&
      (strcasecmp(transmission,"curl")      != 0 ))
    transmission = "file";

  if (strcasecmp(transmission, "file") == 0 || strcasecmp(transmission, "file_separate_dirs") == 0 )
    {
      if (resultFn == NULL)
	{
	  DEBUG0(my_stderr,"    resultFn is NULL, $HOME or $USER might be undefined -> No XALT output\n");
	  return;
	}

      int err = mkpath(resultDir, 0700);
      if (err)
	{
	  if (xalt_tracing)
	    {
	      perror("Error: ");
	      fprintf(my_stderr,"    unable to mkpath(%s) -> No XALT output\n", resultDir);
	    }
	  return;
	}

      char* tmpFn = NULL;
      asprintf(&tmpFn, "%s.%s.new",resultDir, resultFn);
      char* fn = NULL;
      asprintf(&fn, "%s%s",resultDir, resultFn);

      FILE* fp = fopen(tmpFn,"w");
      if (fp == NULL && xalt_tracing)
	fprintf(my_stderr,"    Unable to open: %s -> No XALT output\n", fn);
      else
        {
          fprintf(fp, "%s\n", jsonStr);
          fclose(fp);
          rename(tmpFn, fn);
          DEBUG2(my_stderr,"    Wrote json %s file : %s\n",kind, fn);
        }
      my_free(tmpFn,strlen(tmpFn));
      my_free(fn,   strlen(fn));
    }
  else if (strcasecmp(transmission, "syslogv1") == 0)
    {
      int   zslen;
      int   b64len;
      char* zs      = compress_string(jsonStr,&zslen);
      char* b64     = base64_encode(zs, zslen, &b64len);

      asprintf(&cmdline, "LD_PRELOAD= XALT_EXECUTABLE_TRACKING=no PATH=%s logger -t XALT_LOGGING_%s \"%s:%s\"\n",XALT_SYSTEM_PATH, syshost, kind, b64);
      system(cmdline);
      my_free(zs,zslen);
      my_free(b64,b64len);
      my_free(cmdline,strlen(cmdline));
    }
  else if (strcasecmp(transmission, "logger") == 0)
    {
      int   sz;
      int   zslen;
      char* zs      = compress_string(jsonStr, &zslen);
      char* b64     = base64_encode(zs, zslen, &sz);
      
      int   blkSz   = (sz < syslog_msg_sz) ? sz : syslog_msg_sz;
      int   nBlks   = (sz -  1)/blkSz + 1;
      int   istrt   = 0;
      int   iend    = blkSz;
      int   i;

      for (i = 0; i < nBlks; i++)
        {
          asprintf(&cmdline, "LD_PRELOAD= XALT_EXECUTABLE_TRACKING=no PATH=%s logger -t XALT_LOGGING_%s V:2 kind:%s idx:%d nb:%d syshost:%s key:%s value:%.*s\n",
                   XALT_SYSTEM_PATH, syshost, kind, i, nBlks, syshost, key, iend-istrt, &b64[istrt]);
          system(cmdline);
          my_free(cmdline, strlen(cmdline));
          istrt = iend;
          iend  = istrt + blkSz;
          if (iend > sz)
            iend = sz;
        }
      my_free(b64, sz);
      my_free(zs,  zslen);
    }
  else if (strcasecmp(transmission, "syslog") == 0)
    {
      int   sz;
      int   zslen;
      char* zs      = compress_string(jsonStr, &zslen);
      char* b64     = base64_encode(zs, zslen, &sz);
      
      int   blkSz   = (sz < syslog_msg_sz) ? sz : syslog_msg_sz;
      int   nBlks   = (sz -  1)/blkSz + 1;
      int   istrt   = 0;
      int   iend    = blkSz;
      int   i;

      asprintf(&logNm, "XALT_LOGGING_%s",syshost);
      openlog(logNm, 0, LOG_USER);

      for (i = 0; i < nBlks; i++)
        {
          syslog(LOG_INFO, "V:2 kind:%s idx:%d nb:%d syshost:%s key:%s value:%.*s",
                   kind, i, nBlks, syshost, key, iend-istrt, &b64[istrt]);
          istrt = iend;
          iend  = istrt + blkSz;
          if (iend > sz)
            iend = sz;
        }
      closelog();
      my_free(b64,   sz);
      my_free(zs,    zslen);
      my_free(logNm, strlen(logNm));
    }
  else if (strcasecmp(transmission, "curl") == 0)
    {
      int pid, status, ret = 0;
      char *myargs [] 	= { NULL, NULL, NULL, NULL};
      char *myenv  [] 	= { NULL };

      // Prepend $LIB64 to $LD_LIBRARY_PATH
      int  i;
      char *ld_lib_path = getenv("LD_LIBRARY_PATH");
      char *lib64_dir   = xalt_dir("lib64");
      int  len_lpath    = strlen(ld_lib_path);
      int  len_l64dir   = strlen(lib64_dir);
      int  len          = len_l64dir + len_lpath + 2;
      char *value       = (char *) malloc(len*sizeof(char));

      i = 0;
      memcpy(&value[i], lib64_dir,   len_l64dir); i += len_l64dir;
      value[i] = ':';                             i += 1;
      memcpy(&value[i], ld_lib_path, len_lpath);  i += len_lpath;
      value[i] = '\0';                            i += 1;
      setenv("LD_LIBRARY_PATH", value, 1);
      my_free(value,len);

      // Define arguments to xalt_curl_transmit.c
      char* prgm = xalt_dir("libexec/xalt_curl_transmit"); 
      myargs[0]  = prgm;
      myargs[1]  = (char *) syshost;
      myargs[2]  = (char *) jsonStr;

      // Call xalt_curl_transmit as a child process and wait for it to complete.
      pid = fork();
      if (pid == 0)
	{
	  // Child process
	  execve(prgm, myargs, myenv);
	}

      // Parent: Wait for child to complete
      if ((ret = waitpid (pid, &status, 0)) != 0)
	{
	  DEBUG0(my_stderr, "  waitpid() returned -1: error with xalt_curl_transmit\n");
	  asprintf(&logNm, "XALT_LOGGING_ERROR_%s",syshost);
	  openlog(logNm, LOG_PID, LOG_USER);
	  syslog(LOG_INFO, "waitpid() returned -1: error with xalt_curl_transmit");
	  closelog();
	  my_free(logNm,strlen(logNm));
	  return;
	}

      // Free memory
      my_free(prgm, strlen(prgm));
    }
}
