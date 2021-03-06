/*
*This material was prepared by the Los Alamos National Security, LLC (LANS) under
*Contract DE-AC52-06NA25396 with the U.S. Department of Energy (DOE). All rights
*in the material are reserved by DOE on behalf of the Government and LANS
*pursuant to the contract. You are authorized to use the material for Government
*purposes but it is not to be released or distributed to the public. NEITHER THE
*UNITED STATES NOR THE UNITED STATES DEPARTMENT OF ENERGY, NOR THE LOS ALAMOS
*NATIONAL SECURITY, LLC, NOR ANY OF THEIR EMPLOYEES, MAKES ANY WARRANTY, EXPRESS
*OR IMPLIED, OR ASSUMES ANY LEGAL LIABILITY OR RESPONSIBILITY FOR THE ACCURACY,
*COMPLETENESS, OR USEFULNESS OF ANY INFORMATION, APPARATUS, PRODUCT, OR PROCESS
*DISCLOSED, OR REPRESENTS THAT ITS USE WOULD NOT INFRINGE PRIVATELY OWNED RIGHTS.
*/

#include "config.h"
#include <fcntl.h>
#include <errno.h>

#include "pfutils.h"
#include "debug.h"

#include <syslog.h>
#include <signal.h>

#ifdef THREADS_ONLY
#include <pthread.h>
#include "mpii.h"
#define MPI_Abort MPY_Abort
#define MPI_Pack MPY_Pack
#define MPI_Unpack MPY_Unpack
#endif


/**
* Prints the usage for pftool.
*/
void usage () {
    // print usage statement
    printf ("********************** PFTOOL USAGE ************************************************************\n");
    printf ("\n");
    printf ("\npftool: parallel file tool utilities\n");
    printf ("1. Walk through directory tree structure and gather statistics on files and\n");
    printf ("   directories encountered.\n");
    printf ("2. Apply various data moving operations based on the selected options \n");
    printf ("\n");
    printf ("mpirun -np totalprocesses pftool [options]\n");
    printf (" Options\n");
    printf (" [-p]                                      : path to start parallel tree walk (required argument)\n");
    printf (" [-c]                                      : destination path for data movement\n");
    printf (" [-j]                                      : unique jobid for the pftool job\n");
    printf (" [-w]                                      : work type: copy, list, or compare\n");
    printf (" [-i]                                      : process paths in a file list instead of walking the file system\n");
    printf (" [-s]                                      : block size for copy and compare\n");
    printf (" [-C]                                      : file size to start chunking (n to 1)\n");
    printf (" [-S]                                      : chunk size for copy\n");
#ifdef FUSE_CHUNKER
    printf (" [-f]                                      : path to FUSE directory\n");
    printf (" [-d]                                      : number of directories used for FUSE backend\n");
    printf (" [-W]                                      : file size to start FUSE chunking\n");
    printf (" [-A]                                      : FUSE chunk size for copy\n");
#endif
    printf (" [-n]                                      : operate on file if different\n");
    printf (" [-r]                                      : recursive operation down directory tree\n");
    printf (" [-t]                                      : specify file system type of destination file/directory\n");
    printf (" [-l]                                      : turn on logging to /var/log/mesages\n");
    printf (" [-P]                                      : force destination filesystem to be treated as parallel\n");
    printf (" [-M]                                      : perform block compare, default: metadata compare\n");
#ifdef GEN_SYNDATA
    printf (" [-X]                                      : specify a synthetic data pattern file or constant default: none\n");
    printf (" [-x]                                      : synthetic file size. If specified, file(s) will be synthetic data of specified size\n");
#endif
    printf (" [-v]                                      : user verbose output\n");
    printf (" [-h]                                      : print Usage information\n");
    printf (" \n");
    printf ("********************** PFTOOL USAGE ************************************************************\n");
    return;
}

/**
* Returns the PFTOOL internal command in string format.
* See pfutils.h for the list of commands.
*
* @param cmdidx		the command (or command type)
*
* @return a string representation of the command
*/
char *cmd2str(enum cmd_opcode cmdidx) {
	static char *CMDSTR[] = {
			 "EXITCMD"
			,"UPDCHUNKCMD"
			,"OUTCMD"
			,"BUFFEROUTCMD"
			,"LOGCMD"
			,"QUEUESIZECMD"
			,"STATCMD"
			,"COMPARECMD"
			,"COPYCMD"
			,"PROCESSCMD"
			,"INPUTCMD"
			,"DIRCMD"
#ifdef TAPE
			,"TAPECMD"
			,"TAPESTATCMD"
#endif
			,"WORKDONECMD"
			,"NONFATALINCCMD"
			,"CHUNKBUSYCMD"
			,"COPYSTATSCMD"
			,"EXAMINEDSTATSCMD"
				};

	return((cmdidx > EXAMINEDSTATSCMD)?"Invalid Command":CMDSTR[cmdidx]);
}

char *printmode (mode_t aflag, char *buf) {
    // print the mode in a regular 'pretty' format
    static int m0[] = { 1, S_IREAD >> 0, 'r', '-' };
    static int m1[] = { 1, S_IWRITE >> 0, 'w', '-' };
    static int m2[] = { 3, S_ISUID | S_IEXEC, 's', S_IEXEC, 'x', S_ISUID, 'S', '-' };
    static int m3[] = { 1, S_IREAD >> 3, 'r', '-' };
    static int m4[] = { 1, S_IWRITE >> 3, 'w', '-' };
    static int m5[] = { 3, S_ISGID | (S_IEXEC >> 3), 's',
                        S_IEXEC >> 3, 'x', S_ISGID, 'S', '-'
                      };
    static int m6[] = { 1, S_IREAD >> 6, 'r', '-' };
    static int m7[] = { 1, S_IWRITE >> 6, 'w', '-' };
    static int m8[] = { 3, S_ISVTX | (S_IEXEC >> 6), 't', S_IEXEC >> 6, 'x', S_ISVTX, 'T', '-' };
    static int *m[] = { m0, m1, m2, m3, m4, m5, m6, m7, m8 };
    int i, j, n;
    int *p = (int *) 1;;
    buf[0] = S_ISREG (aflag) ? '-' : S_ISDIR (aflag) ? 'd' : S_ISLNK (aflag) ? 'l' : S_ISFIFO (aflag) ? 'p' : S_ISCHR (aflag) ? 'c' : S_ISBLK (aflag) ? 'b' : S_ISSOCK (aflag) ? 's' : '?';
    for (i = 0; i <= 8; i++) {
        for (n = m[i][0], j = 1; n > 0; n--, j += 2) {
            p = m[i];
            if ((aflag & p[j]) == p[j]) {
                j++;
                break;
            }
        }
        buf[i + 1] = p[j];
    }
    buf[10] = '\0';
    return buf;
}

void hex_dump_bytes (char *b, int len, char *outhexbuf) {
    short str_index;
    char smsg[64];
    char tmsg[3];
    unsigned char *ptr;
    int start = 0;
    ptr = (unsigned char *) (b + start);  /* point to buffer location to start  */
    /* if last frame and more lines are required get number of lines */
    memset (smsg, '\0', 64);
    for (str_index = 0; str_index < 28; str_index++) {
        sprintf (tmsg, "%02X", ptr[str_index]);
        strncat (smsg, tmsg, 2);
    }
    sprintf (outhexbuf, "%s", smsg);
}

/**
* This function walks the path, and creates all elements in 
* the path as a directory - if they do not exist. It
* basically does a "mkdir -p" programatically.
*
* @param thePath	the path to test and create
* @param perms		the permission mode to use when
* 			creating directories in this path
*
* @return 0 if all directories are succesfully created.
* 	errno (i.e. non-zero) if there is an error. 
* 	See "man -s 2 mkdir" for error description.
*/
int mkpath(char *thePath, mode_t perms) {
	char *slash = thePath;				// point at the current "/" in the path
	struct stat sbuf;				// a buffer to hold stat information
	int save_errno;					// errno from mkdir()

	while( *slash == '/') slash++;			// burn through any leading "/". Note that if no leading "/",
							// then thePath will be created relative to CWD of process.
	while(slash = strchr(slash,'/')) {		// start parsing thePath
	  *slash = '\0';
	  
	  if(stat(thePath,&sbuf)) {			// current path element cannot be stat'd - assume does not exist
	    if(mkdir(thePath,perms)) {			// problems creating the directory - clean up and return!
	      save_errno = errno;			// save off errno - in case of error...
	      *slash = '/';
	      return(save_errno);
	    }
	  }
	  else if (!S_ISDIR(sbuf.st_mode)) {		// element exists but is NOT a directory
	    *slash = '/';
	    return(ENOTDIR);
	  }
	  *slash = '/';slash++;				// increment slash ...
	  while( *slash == '/') slash++;		// burn through any blank path elements
	} // end mkdir loop

	if(stat(thePath,&sbuf)) {			// last path element cannot be stat'd - assume does not exist
	  if(mkdir(thePath,perms))			// problems creating the directory - clean up and return!
	    return(save_errno = errno);			// save off errno - just to be sure ...
	}
	else if (!S_ISDIR(sbuf.st_mode))		// element exists but is NOT a directory
	  return(ENOTDIR);

	return(0);
}

/**
* Low Level utility function to write a field of a data
* structure - any data structure.
*
* @param fd		the open file descriptor
* @param start		the starting memory address
* 			(pointer) of the field
* @param len		the length of the filed in bytes
*
* @return number of bytes written, If return
* 	is < 0, then there were problems writing,
* 	and the number can be taken as the errno.
*/
ssize_t write_field(int fd, void *start, size_t len) {
	size_t n;					// number of bytes written for a given call to write()
	ssize_t tot = 0;				// total number of bytes written
	void *wstart = start;				// the starting point in the buffer
	size_t wcnt = len;				// the running count of bytes to write

	while(wcnt > 0) {
	  if(!(n=write(fd,wstart,wcnt)))		// if nothing written -> assume error
	    return((ssize_t)-errno);
	  tot += n;
	  wstart += n;					// incremant the start address by n
	  wcnt -= n;					// decreamnt byte count by n
	}

	return(tot);
}

#ifdef TAPE
int read_inodes (const char *fnameP, gpfs_ino_t startinode, gpfs_ino_t endinode, int *dmarray) {
    const gpfs_iattr_t *iattrP;
    gpfs_iscan_t *iscanP = NULL;
    gpfs_fssnap_handle_t *fsP = NULL;
    int rc = 0;
    fsP = gpfs_get_fssnaphandle_by_path (fnameP);
    if (fsP == NULL) {
        rc = errno;
        fprintf (stderr, "gpfs_get_fshandle_by_path: %s\n", strerror (rc));
        goto exit;
    }
    iscanP = gpfs_open_inodescan (fsP, NULL, NULL);
    if (iscanP == NULL) {
        rc = errno;
        fprintf (stderr, "gpfs_open_inodescan: %s\n", strerror (rc));
        goto exit;
    }
    if (startinode > 0) {
        rc = gpfs_seek_inode (iscanP, startinode);
        if (rc != 0) {
            rc = errno;
            fprintf (stderr, "gpfs_seek_inode: %s\n", strerror (rc));
            goto exit;
        }
    }
    while (1) {
        rc = gpfs_next_inode (iscanP, endinode, &iattrP);
        if (rc != 0) {
            rc = errno;
            fprintf (stderr, "gpfs_next_inode: %s\n", strerror (rc));
            goto exit;
        }
        if ((iattrP == NULL) || (iattrP->ia_inode > endinode)) {
            break;
        }
        if (iattrP->ia_xperm & GPFS_IAXPERM_DMATTR) {
            dmarray[0] = 1;
        }
    }
exit:
    if (iscanP) {
        gpfs_close_inodescan (iscanP);
    }
    if (fsP) {
        gpfs_free_fssnaphandle (fsP);
    }
    return rc;
}

//dmapi lookup
int dmapi_lookup (char *mypath, int *dmarray, char *dmouthexbuf) {
    char *version;
    char *session_name = "lookupdmapi";
    static dm_sessid_t dump_dmapi_session = DM_NO_SESSION;
    typedef long long offset_t;
    void *dmhandle = NULL;
    size_t dmhandle_len = 0;
    u_int nelemr;
    dm_region_t regbufpr[4000];
    u_int nelempr;
    size_t dmattrsize;
    char dmattrbuf[4000];
    size_t dmattrsizep;
    //struct dm_attrlist my_dm_attrlist[20];
    struct dm_attrlist my_dm_attrlistM[20];
    struct dm_attrlist my_dm_attrlistP[20];
    //struct dm_attrname my_dm_attrname;
    struct dm_attrname my_dm_attrnameM;
    struct dm_attrname my_dm_attrnameP;
    char localhexbuf[128];
    if (dm_init_service (&version) < 0) {
        printf ("Cant get a dmapi session\n");
        exit (-1);
    }
    if (dm_create_session (DM_NO_SESSION, session_name, &dump_dmapi_session) != 0) {
        printf ("create_session \n");
        exit (-1);
    }
    //printf("-------------------------------------------------------------------------------------\n");
    //printf("created new DMAPI session named '%s' for %s\n",session_name,version);
    if (dm_path_to_handle ((char *) mypath, &dmhandle, &dmhandle_len) != 0) {
        goto done;
    }
    /* I probably should get more managed regions and check them all but
       I dont know how to find out how many I will have total
     */
    nelemr = 1;
    if (dm_get_region (dump_dmapi_session, dmhandle, dmhandle_len, DM_NO_TOKEN, nelemr, regbufpr, &nelempr) != 0) {
        printf ("dm_get_region failed\n");
        goto done;
    }
    PRINT_DMAPI_DEBUG ("regbufpr: number of managed regions = %d \n", nelempr);
    PRINT_DMAPI_DEBUG ("regbufpr.rg_offset = %lld \n", regbufpr[0].rg_offset);
    PRINT_DMAPI_DEBUG ("regbufpr.rg_size = %lld \n", regbufpr[0].rg_size);
    PRINT_DMAPI_DEBUG ("regbufpr.rg_flags = %u\n", regbufpr[0].rg_flags);
    if (regbufpr[0].rg_flags > 0) {
        if (regbufpr[0].rg_flags & DM_REGION_READ) {
            PRINT_DMAPI_DEBUG ("regbufpr: File %s is migrated - dmapi wants to be notified on at least read for this region at offset %lld\n", mypath, regbufpr[0].rg_offset);
            dmarray[2] = 1;
            dmattrsize = sizeof (my_dm_attrlistM);
            if (dm_getall_dmattr (dump_dmapi_session, dmhandle, dmhandle_len, DM_NO_TOKEN, dmattrsize, my_dm_attrlistM, &dmattrsizep) != 0) {
                PRINT_DMAPI_DEBUG ("dm_getall_dmattr failed path %s\n", mypath);
                goto done;
            }
            //strncpy((char *) my_dm_attrlistM[0].al_name.an_chars,"\n\n\n\n\n\n\n",7);
            //strncpy((char *) my_dm_attrlistM[0].al_name.an_chars,"IBMObj",6);
            PRINT_DMAPI_DEBUG ("M dm_getall_dmattr attrs %x size %ld\n", my_dm_attrlistM->al_name.an_chars[0], dmattrsizep);
            dmattrsize = sizeof (dmattrbuf);
            strncpy ((char *) my_dm_attrnameM.an_chars, "IBMObj", 6);
            PRINT_DMAPI_DEBUG ("MA dm_get_dmattr attr 0 %s\n", my_dm_attrnameM.an_chars);
            if (dm_get_dmattr (dump_dmapi_session, dmhandle, dmhandle_len, DM_NO_TOKEN, &my_dm_attrnameM, dmattrsize, dmattrbuf, &dmattrsizep) != 0) {
                goto done;
            }
            hex_dump_bytes (dmattrbuf, 28, localhexbuf);
            PRINT_DMAPI_DEBUG ("M dmapi_lookup localhexbuf %s\n", localhexbuf);
            strncpy (dmouthexbuf, localhexbuf, 128);
        }
        else {
            // This is a pre-migrated file
            //
            PRINT_DMAPI_DEBUG ("regbufpr: File %s is premigrated  - dmapi wants to be notified on write and/or trunc for this region at offset %lld\n", mypath, regbufpr[0].rg_offset);
            // HB 0324-2009
            dmattrsize = sizeof (my_dm_attrlistP);
            if (dm_getall_dmattr (dump_dmapi_session, dmhandle, dmhandle_len, DM_NO_TOKEN, dmattrsize, my_dm_attrlistP, &dmattrsizep) != 0) {
                PRINT_DMAPI_DEBUG ("P dm_getall_dmattr failed path %s\n", mypath);
                goto done;
            }
            //strncpy((char *) my_dm_attrlistP[0].al_name.an_chars,"IBMPMig",7);
            PRINT_DMAPI_DEBUG ("P dm_getall_dmattr attrs %x size %ld\n", my_dm_attrlistP->al_name.an_chars[0], dmattrsizep);
            dmattrsize = sizeof (dmattrbuf);
            strncpy ((char *) my_dm_attrnameP.an_chars, "IBMPMig", 7);
            PRINT_DMAPI_DEBUG ("PA dm_get_dmattr attr 0 %s\n", my_dm_attrnameP.an_chars);
            if (dm_get_dmattr (dump_dmapi_session, dmhandle, dmhandle_len, DM_NO_TOKEN, &my_dm_attrnameP, dmattrsize, dmattrbuf, &dmattrsizep) != 0) {
                goto done;
            }
            hex_dump_bytes (dmattrbuf, 28, localhexbuf);
            strncpy (dmouthexbuf, localhexbuf, 128);
            dmarray[1] = 1;
        }
    }
    else {
        // printf("regbufpr: File is resident - no managed regions\n");
        dmarray[0] = 1;
    }
done:
    /* I know this done goto is crap but I am tired
       we now free up the dmapi resources and set our uid back to
       the original user
     */
    dm_handle_free (dmhandle, dmhandle_len);
    if (dm_destroy_session (dump_dmapi_session) != 0) {
    }
    //printf("-------------------------------------------------------------------------------------\n");
    return (0);
}
#endif


char *get_base_path(const char *path, int wildcard) {
    //wild card is a boolean
    char base_path[PATHSIZE_PLUS], dir_name[PATHSIZE_PLUS];
    struct stat st;
    int rc;
#ifdef PLFS
    rc = plfs_getattr(NULL, path, &st, 0);
    if (rc != 0){
#endif
        rc = lstat(path, &st);
#ifdef PLFS
    }
#endif
    if (rc < 0) {
        fprintf(stderr, "Failed to stat path %s\n", path);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    strncpy(dir_name, dirname(strdup(path)), PATHSIZE_PLUS);
    if (strncmp(".", dir_name, PATHSIZE_PLUS == 0) && S_ISDIR(st.st_mode)) {
        strncpy(base_path, path, PATHSIZE_PLUS);
    }
    else if (S_ISDIR(st.st_mode) && wildcard == 0) {
        strncpy(base_path, path, PATHSIZE_PLUS);
    }
    else {
        strncpy(base_path, dir_name, PATHSIZE_PLUS);
    }
    while(base_path[strlen(base_path) - 1] == '/') {
        base_path[strlen(base_path) - 1] = '\0';
    }
    return strdup(base_path);
}

void get_dest_path(path_item beginning_node, const char *dest_path, path_item *dest_node, int makedir, int num_paths, struct options o) {
    int rc;
    struct stat beg_st, dest_st;
    char temp_path[PATHSIZE_PLUS], final_dest_path[PATHSIZE_PLUS];
    char *path_slice;

    strncpy(final_dest_path, dest_path, PATHSIZE_PLUS);
    strncpy(temp_path, beginning_node.path, PATHSIZE_PLUS);
    while (temp_path[strlen(temp_path)-1] == '/') {
        temp_path[strlen(temp_path)-1] = '\0';
    }
    //recursion special cases
    if (o.recurse && strncmp(temp_path, "..", PATHSIZE_PLUS) != 0 && o.work_type != COMPAREWORK) {
        beg_st = beginning_node.st;
#ifdef PLFS
    rc = plfs_getattr(NULL, dest_path, &dest_st, 0);
    if (rc != 0){
#endif
        rc = lstat(dest_path, &dest_st);
#ifdef PLFS
    }
#endif
        if (rc >= 0 && S_ISDIR(dest_st.st_mode) && S_ISDIR(beg_st.st_mode) && num_paths == 1) {
            if (strstr(temp_path, "/") == NULL) {
                path_slice = (char *)temp_path;
            }
            else {
                path_slice = strrchr(temp_path, '/') + 1;
            }
            if (final_dest_path[strlen(final_dest_path)-1] != '/') {
                strncat(final_dest_path, "/", PATHSIZE_PLUS);
            }
            strncat(final_dest_path, path_slice, PATHSIZE_PLUS - strlen(final_dest_path) - 1);
        }
    }
    rc = lstat(final_dest_path, &dest_st);
    if (rc >= 0) {
        (*dest_node).st = dest_st;
    }
    else {
        (*dest_node).st.st_mode = 0;
    }
    strncpy((*dest_node).path, final_dest_path, PATHSIZE_PLUS);
}

char *get_output_path(const char *base_path, path_item src_node, path_item dest_node, struct options o) {
    char output_path[PATHSIZE_PLUS];
    char *path_slice;
    //remove a trailing slash
    strncpy(output_path, dest_node.path, PATHSIZE_PLUS);

    while (output_path[strlen(output_path) - 1] == '/') {
        output_path[strlen(output_path) - 1] = '\0';
    }
    //path_slice = strstr(src_path, base_path);
    if (o.recurse == 0) {
        if(strstr(src_node.path, "/") != NULL) {
            path_slice = (char *) strrchr(src_node.path, '/')+1;
        }
        else {
            path_slice = (char *) src_node.path;
        }
    }
    else {
        if (strncmp(base_path, ".", PATHSIZE_PLUS) == 0) {
            path_slice = (char *) src_node.path;
        }
        else {
            path_slice = strdup(src_node.path + strlen(base_path) + 1);
        }
    }
    if (S_ISDIR(dest_node.st.st_mode)) {
        strncat(output_path, "/", PATHSIZE_PLUS);
        strncat(output_path, path_slice, PATHSIZE_PLUS - strlen(output_path) - 1);
    }
    return strdup(output_path);
}

int one_byte_read(const char *path) {
    int fd, bytes_processed;
    char data;
    int rc = 0;
    char errormsg[MESSAGESIZE];
    fd = open(path, O_RDONLY);
    if (fd < 0) {
        sprintf(errormsg, "Failed to open file %s for read", path);
        errsend(NONFATAL, errormsg);
        return -1;
    }
    bytes_processed = read(fd, &data, 1);
    if (bytes_processed != 1) {
        sprintf(errormsg, "%s: Read %d bytes instead of %d", path, bytes_processed, 1);
        errsend(NONFATAL, errormsg);
        return -1;
    }
    rc = close(fd);
    if (rc != 0) {
        sprintf(errormsg, "Failed to close file: %s", path);
        errsend(NONFATAL, errormsg);
        return -1;
    }
    return 0;
}

#ifdef GEN_SYNDATA
int copy_file(path_item src_file, path_item dest_file, size_t blocksize, syndata_buffer *synbuf, int rank) {
#else
int copy_file(path_item src_file, path_item dest_file, size_t blocksize, int rank) {
#endif
    //take a src, dest, offset and length. Copy the file and return 0 on success, -1 on failure
    //MPI_Status status;
    int rc;
    size_t completed = 0;
    char *buf = NULL;
    char errormsg[MESSAGESIZE];
    //FILE *src_fd, *dest_fd;
    int flags;
    //MPI_File src_fd, dest_fd;
    int src_fd, dest_fd = -1;
    off_t offset = (src_file.chkidx * src_file.chksz);	
    off_t length = ((offset+src_file.chksz)>src_file.st.st_size)?(src_file.st.st_size-offset):src_file.chksz;
#ifdef PLFS
    int pid = getpid();
    Plfs_fd  *plfs_src_fd = NULL, *plfs_dest_fd = NULL;
#endif
    size_t bytes_processed = 0;
    //symlink
    char link_path[PATHSIZE_PLUS];
    int numchars;

    //can't be const for MPI_IO
    if (S_ISLNK(src_file.st.st_mode)) {
        numchars = readlink(src_file.path, link_path, PATHSIZE_PLUS);
        if (numchars < 0) {
            sprintf(errormsg, "Failed to read link %s", src_file.path);
            errsend(NONFATAL, errormsg);
            return -1;
        }
        rc = symlink(link_path,dest_file.path);
        if (rc < 0) {
            sprintf(errormsg, "Failed to create symlink %s -> %s", dest_file.path, link_path);
            errsend(NONFATAL, errormsg);
            return -1;
        }
        if (update_stats(src_file, dest_file) != 0) {
            return -1;
        }
        return 0;
    }
    if (length < blocksize) {										// a file < blocksize in size
        blocksize = length;
    }
    if (blocksize) {											// if a non-zero length file -> allocate buf - cds 8/2015
       buf = malloc(blocksize * sizeof(char));
       memset(buf, '\0', blocksize);
    }
    //MPI_File_read(src_fd, buf, 2, MPI_BYTE, &status);
    //open the source file for reading in binary mode
    //rc = MPI_File_open(MPI_COMM_SELF, source_file, MPI_MODE_RDONLY, MPI_INFO_NULL, &src_fd);
#ifdef GEN_SYNDATA
    if(!syndataExists(synbuf)) {
#endif
#ifdef PLFS
        if (src_file.ftype == PLFSFILE){
            src_fd = plfs_open(&plfs_src_fd, src_file.path, O_RDONLY, pid+rank, src_file.st.st_mode, NULL);
        }
        else {
#endif
            src_fd = open(src_file.path, O_RDONLY);
#ifdef PLFS
        }
#endif
        if (src_fd < 0) {
            sprintf(errormsg, "Failed to open file %s for read", src_file.path);
            errsend(NONFATAL, errormsg);
            return -1;
        }
#ifdef GEN_SYNDATA
    }
#endif
    PRINT_IO_DEBUG("rank %d: copy_file() Copying chunk index %d. offset = %ld   length = %ld   blocksize = %ld\n", rank, src_file.chkidx, offset, length, blocksize);

    //first create a file and open it for appending (file doesn't exist)
    //rc = MPI_File_open(MPI_COMM_SELF, destination_file, MPI_MODE_CREATE | MPI_MODE_WRONLY, MPI_INFO_NULL, &dest_fd);
    if ((src_file.st.st_size == length && offset == 0) || strncmp(dest_file.fstype,"panfs",5)) {	// no chunking or not writing to PANFS - cds 6/2014
       flags = O_WRONLY | O_CREAT;
       PRINT_IO_DEBUG("rank %d: copy_file() fstype = %s. Setting open flags to O_WRONLY | O_CREAT\n", rank, dest_file.fstype);
    }
    else {												// Panasas FS needs O_CONCURRENT_WRITE set for file writes - cds 6/2014
       flags = O_WRONLY | O_CREAT | O_CONCURRENT_WRITE;
       PRINT_IO_DEBUG("rank %d: copy_file() fstype = %s. Setting open flags to O_WRONLY | O_CREAT | O_CONCURRENT_WRITE\n", rank, dest_file.fstype);
    }
#ifdef PLFS
    if (src_file.desttype == PLFSFILE){
        dest_fd = plfs_open(&plfs_dest_fd, dest_file.path, flags, pid+rank, src_file.st.st_mode, NULL);
    }
    else{
#endif
       	dest_fd = open(dest_file.path, flags, 0600);
#ifdef PLFS
    }
#endif
    if (dest_fd < 0) {
        sprintf(errormsg, "Failed to open file %s for write (errno = %d)", dest_file.path, errno);
        errsend(NONFATAL, errormsg);
        return -1;
    }

    while (completed != length) {
        //1 MB is too big
        if ((length - completed) < blocksize) {
            blocksize = (length - completed);
        }
        //rc = MPI_File_read_at(src_fd, completed, buf, blocksize, MPI_BYTE, &status);
        memset(buf, '\0', blocksize);
#ifdef GEN_SYNDATA
        if(!syndataExists(synbuf)) {
#endif
#ifdef PLFS
           if (src_file.ftype == PLFSFILE) {
               bytes_processed = plfs_read(plfs_src_fd, buf, blocksize, completed+offset);
           }
           else {
#endif
               bytes_processed = pread(src_fd, buf, blocksize, completed+offset);
#ifdef PLFS
           }
#endif
#ifdef GEN_SYNDATA
        }
        else {
	    int buflen = blocksize * sizeof(char);		// Make sure buffer length is the right size!

            if(bytes_processed = syndataFill(synbuf,buf,buflen)) {
               sprintf(errormsg, "Failed to copy from synthetic data buffer. err = %d", bytes_processed);
               errsend(NONFATAL, errormsg);
	       return -1;
	    }
	    bytes_processed = buflen;				// On a successful call to syndataFill(), bytes_processed equals 0
        }
#endif
        if (bytes_processed != blocksize) {
            sprintf(errormsg, "%s: Read %ld bytes instead of %zd", src_file.path, bytes_processed, blocksize);
            errsend(NONFATAL, errormsg);
            return -1;
        }
        //rc = MPI_File_write_at(dest_fd, completed, buf, blocksize, MPI_BYTE, &status );
#ifdef PLFS
        if (src_file.desttype == PLFSFILE) {
            bytes_processed = plfs_write(plfs_dest_fd, buf, blocksize, completed+offset, pid);
        }
        else {
#endif
            bytes_processed = pwrite(dest_fd, buf, blocksize, completed+offset);
#ifdef PLFS
        }
#endif
        if (bytes_processed != blocksize) {
            sprintf(errormsg, "%s: write %ld bytes instead of %zd", dest_file.path, bytes_processed, blocksize);
            errsend(NONFATAL, errormsg);
            return -1;
        }
        completed += blocksize;
    }
    PRINT_IO_DEBUG("rank %d: copy_file() Copy of %d bytes complete for file %s\n", rank, bytes_processed, dest_file.path);
#ifdef GEN_SYNDATA
    if(!syndataExists(synbuf)) {
#endif
#ifdef PLFS
       if (src_file.ftype == PLFSFILE) {
           rc = plfs_close(plfs_src_fd, pid+rank, src_file.st.st_uid, O_RDONLY, NULL);
       }
       else {
#endif
           rc = close(src_fd);
           if (rc != 0) {
               sprintf(errormsg, "Failed to close file: %s", src_file.path);
               errsend(NONFATAL, errormsg);
               return -1;
           }
#ifdef PLFS
       }
#endif
#ifdef GEN_SYNDATA
    }
#endif
#ifdef PLFS
    if (src_file.desttype == PLFSFILE) {
        rc = plfs_close(plfs_dest_fd, pid+rank, src_file.st.st_uid, flags, NULL);
    }
    else {
#endif
        if (close(dest_fd) < 0) {									// Report error if problems closing
            sprintf(errormsg, "Failed to close file: %s (errno = %d)", dest_file.path, errno);
            errsend(NONFATAL, errormsg);
            return -1;
        }
#ifdef PLFS
    }
#endif
    if(buf) free(buf);											// if buf has been allocated -> free it - cds 8/2015
    if (offset == 0 && length == src_file.st.st_size) {
        PRINT_IO_DEBUG("rank %d: copy_file() Updating transfer stats for %s\n", rank, dest_file.path);
        if (update_stats(src_file, dest_file) != 0) {
            return -1;
        }
    }
    return 0;
}


int compare_file(path_item src_file, path_item dest_file, size_t blocksize, int meta_data_only) {
    struct stat dest_st;
    size_t completed = 0;
    char *ibuf;
    char *obuf;
    int src_fd, dest_fd;
    size_t bytes_processed;
    char errormsg[MESSAGESIZE];
    int rc;
    int crc;
    off_t offset = src_file.chkidx*src_file.chksz;
    size_t length = src_file.st.st_size;

    // dest doesn't exist
#ifdef FUSE_CHUNKER
    if (dest_file.ftype == FUSEFILE){
      if (stat(dest_file.path, &dest_st) == -1){
        return 2;
      }
    }
    else{
#endif
      if (lstat(dest_file.path, &dest_st) == -1) {
          return 2;
      }
#ifdef FUSE_CHUNKER
    }
#endif
    if (src_file.st.st_size == dest_st.st_size &&
            (src_file.st.st_mtime == dest_st.st_mtime  ||
             S_ISLNK(src_file.st.st_mode))&&
            src_file.st.st_mode == dest_st.st_mode &&
            src_file.st.st_uid == dest_st.st_uid &&
            src_file.st.st_gid == dest_st.st_gid) {
        //metadata compare
        if (meta_data_only) {
            return 0;
        }
        //byte compare
        ibuf = malloc(blocksize * sizeof(char));
        obuf = malloc(blocksize * sizeof(char));
        src_fd = open(src_file.path, O_RDONLY);
        if (src_fd < 0) {
            sprintf(errormsg, "Failed to open file %s for compare source", src_file.path);
            errsend(NONFATAL, errormsg);
            return -1;
        }
        dest_fd = open(dest_file.path, O_RDONLY);
        if (dest_fd < 0) {
            sprintf(errormsg, "Failed to open file %s for compare destination", dest_file.path);
            errsend(NONFATAL, errormsg);
            return -1;
        }
        //incase someone accidently set and offset+length that exceeds the file bounds
        if ((src_file.st.st_size - offset) < length) {
            length = src_file.st.st_size - offset;
        }
        //a file less then blocksize
        if (length < blocksize) {
            blocksize = length;
        }
        crc = 0;
        while (completed != length) {
            memset(ibuf, 0, blocksize);
            memset(obuf, 0, blocksize);
            //blocksize is too big
            if ((length - completed) < blocksize) {
                blocksize = (length - completed);
            }
            bytes_processed = pread(src_fd, ibuf, blocksize, completed+offset);
            if (bytes_processed != blocksize) {
                sprintf(errormsg, "%s: Read %zd bytes instead of %zd for compare", src_file.path, bytes_processed, blocksize);
                errsend(NONFATAL, errormsg);
                return -1;
            }
            bytes_processed = pread(dest_fd, obuf, blocksize, completed+offset);
            if (bytes_processed != blocksize) {
                sprintf(errormsg, "%s: Read %zd bytes instead of %zd for compare", dest_file.path, bytes_processed, blocksize);
                errsend(NONFATAL, errormsg);
                return -1;
            }
            //compare - if no compare crc=1 if compare crc=0 and get out of loop
            crc = memcmp(ibuf,obuf,blocksize);
            //printf("compare_file: src %s dest %s offset %ld len %d crc %d\n",src_file, dest_file, completed+offset, blocksize, crc);
            if (crc != 0) {
                completed=length;
            }
            completed += blocksize;
        }
        rc = close(src_fd);
        if (rc != 0) {
            sprintf(errormsg, "Failed to close file: %s", src_file.path);
            errsend(NONFATAL, errormsg);
            return -1;
        }
        rc = close(dest_fd);
        if (rc != 0) {
            sprintf(errormsg, "Failed to close file: %s", dest_file.path);
            errsend(NONFATAL, errormsg);
            return -1;
        }
        free(ibuf);
        free(obuf);
        if (crc != 0) {
            return 1;
        }
        else {
            return 0;
        }
    }
    else {
        return 1;
    }
    return 0;
}

int update_stats(path_item src_file, path_item dest_file) {
    int rc;
    char errormsg[MESSAGESIZE];
    int mode;
    struct utimbuf ut;
#ifdef PLFS
    if (src_file.desttype == PLFSFILE){
        rc = plfs_chown(dest_file.path, src_file.st.st_uid, src_file.st.st_gid);
    }
    else{
#endif
        rc = lchown(dest_file.path, src_file.st.st_uid, src_file.st.st_gid);
#ifdef PLFS
    }   
#endif
    if (rc != 0) {
        sprintf(errormsg, "Failed to change ownership of file: %s to %d:%d", dest_file.path, src_file.st.st_uid, src_file.st.st_gid);
        errsend(NONFATAL, errormsg);
    }
    if (!S_ISLNK(src_file.st.st_mode)) {
#ifdef FUSE_CHUNKER
        if (src_file.desttype == FUSEFILE){
            rc = chown(dest_file.path, src_file.st.st_uid, src_file.st.st_gid);
            if (rc != 0) {
                sprintf(errormsg, "Failed to change ownership of fuse chunked file: %s to %d:%d", dest_file.path, src_file.st.st_uid, src_file.st.st_gid);
                errsend(NONFATAL, errormsg);
            }
        }
#endif
        mode = src_file.st.st_mode & 07777;
#ifdef PLFS
        if (src_file.desttype == PLFSFILE){
            rc = plfs_chmod(dest_file.path, mode);
        }
        else{
#endif
            rc = chmod(dest_file.path, mode);
#ifdef PLFS
        }
#endif
        if (rc != 0) {
            sprintf(errormsg, "Failed to chmod file: %s to %o", dest_file.path, mode);
            errsend(NONFATAL, errormsg);
        }
        ut.actime = src_file.st.st_atime;
        ut.modtime = src_file.st.st_mtime;
#ifdef PLFS
        if (src_file.desttype == PLFSFILE){
            rc = plfs_utime(dest_file.path, &ut);
        }
        else{
#endif
            rc = utime(dest_file.path, &ut);
#ifdef PLFS
        }
#endif
        if (rc != 0) {
            sprintf(errormsg, "Failed to set atime and mtime for file: %s", dest_file.path);
            errsend(NONFATAL, errormsg);
        }
    }
    return 0;
}

//local functions only
int request_response(int type_cmd) {
    MPI_Status status;
    int response;
    send_command(MANAGER_PROC, type_cmd);
    if (MPI_Recv(&response, 1, MPI_INT, MANAGER_PROC, MPI_ANY_TAG, MPI_COMM_WORLD, &status) != MPI_SUCCESS) {
        errsend(FATAL, "Failed to receive response\n");
    }
    return response;
}

int request_input_queuesize() {
    return request_response(QUEUESIZECMD);
}

void send_command(int target_rank, int type_cmd) {
    //Tell a rank it's time to begin processing
//    PRINT_MPI_DEBUG("target rank %d: Sending command %s to target rank %d\n", target_rank, cmd2str(type_cmd), target_rank);
    if (MPI_Send(&type_cmd, 1, MPI_INT, target_rank, target_rank, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send command %d to rank %d\n", type_cmd, target_rank);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}


void send_path_list(int target_rank, int command, int num_send, path_list **list_head, path_list **list_tail, int *list_count) {
    int path_count = 0, position = 0;
    int worksize, workcount;
    if (num_send <= *list_count) {
        workcount = num_send;
    }
    else {
        workcount = *list_count;
    }
    worksize = workcount * sizeof(path_item);
    char *workbuf = (char *) malloc(worksize * sizeof(char));
    while(path_count < workcount) {
        path_count++;
        MPI_Pack(&(*list_head)->data, sizeof(path_item), MPI_CHAR, workbuf, worksize, &position, MPI_COMM_WORLD);
        dequeue_node(list_head, list_tail, list_count);
    }
    //send the command to get started
    send_command(target_rank, command);
    //send the # of paths
    if (MPI_Send(&workcount, 1, MPI_INT, target_rank, target_rank, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send workcount %d to rank %d\n", workcount, target_rank);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    if (MPI_Send(workbuf, worksize, MPI_PACKED, target_rank, target_rank, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send workbuf to rank %d\n",  target_rank);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    free(workbuf);
}

void send_path_buffer(int target_rank, int command, path_item *buffer, int *buffer_count) {
    int i;
    int position = 0;
    int worksize;
    char *workbuf;
    path_item work_node;
    worksize = *buffer_count * sizeof(path_item);
    workbuf = (char *) malloc(worksize * sizeof(char));
    for (i = 0; i < *buffer_count; i++) {
        work_node = buffer[i];
        MPI_Pack(&work_node, sizeof(path_item), MPI_CHAR, workbuf, worksize, &position, MPI_COMM_WORLD);
    }
    send_command(target_rank, command);
    if (MPI_Send(buffer_count, 1, MPI_INT, target_rank, target_rank, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send buffer_count %d to rank %d\n", *buffer_count, target_rank);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    if (MPI_Send(workbuf, worksize, MPI_PACKED, target_rank, target_rank, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send workbuf to rank %d\n", target_rank);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    *buffer_count = 0;
    free(workbuf);
}

void send_buffer_list(int target_rank, int command, work_buf_list **workbuflist, int *workbufsize) {
    int size = (*workbuflist)->size;
    int worksize = sizeof(path_item) * size;
    send_command(target_rank, command);
    if (MPI_Send(&size, 1, MPI_INT, target_rank, target_rank, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send workbuflist size %d to rank %d\n", size, target_rank);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    if (MPI_Send((*workbuflist)->buf, worksize, MPI_PACKED, target_rank, target_rank, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send workbuflist buf to rank %d\n", target_rank);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    dequeue_buf_list(workbuflist, workbufsize);
}

//manager
void send_manager_nonfatal_inc() {
    send_command(MANAGER_PROC, NONFATALINCCMD);
}

void send_manager_chunk_busy() {
    send_command(MANAGER_PROC, CHUNKBUSYCMD);
}

void send_manager_copy_stats(int num_copied_files, size_t num_copied_bytes) {
    send_command(MANAGER_PROC, COPYSTATSCMD);
    //send the # of paths
    if (MPI_Send(&num_copied_files, 1, MPI_INT, MANAGER_PROC, MANAGER_PROC, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send num_copied_files %d to rank %d\n", num_copied_files, MANAGER_PROC);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    //send the # of paths
    if (MPI_Send(&num_copied_bytes, 1, MPI_DOUBLE, MANAGER_PROC, MANAGER_PROC, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send num_copied_byes %zd to rank %d\n", num_copied_bytes, MANAGER_PROC);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}

void send_manager_examined_stats(int num_examined_files, size_t num_examined_bytes, int num_examined_dirs) {
    send_command(MANAGER_PROC, EXAMINEDSTATSCMD);
    //send the # of paths
    if (MPI_Send(&num_examined_files, 1, MPI_INT, MANAGER_PROC, MANAGER_PROC, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send num_examined_files %d to rank %d\n", num_examined_files, MANAGER_PROC);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    if (MPI_Send(&num_examined_bytes, 1, MPI_DOUBLE, MANAGER_PROC, MANAGER_PROC, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send num_examined_bytes %zd to rank %d\n", num_examined_bytes, MANAGER_PROC);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    if (MPI_Send(&num_examined_dirs, 1, MPI_INT, MANAGER_PROC, MANAGER_PROC, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send num_examined_dirs %d to rank %d\n", num_examined_dirs, MANAGER_PROC);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}

#ifdef TAPE
void send_manager_tape_stats(int num_examined_tapes, size_t num_examined_tape_bytes) {
    send_command(MANAGER_PROC, TAPESTATCMD);
    //send the # of paths
    if (MPI_Send(&num_examined_tapes, 1, MPI_INT, MANAGER_PROC, MANAGER_PROC, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send num_examined_tapes %d to rank %d\n", num_examined_tapes, MANAGER_PROC);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    if (MPI_Send(&num_examined_tape_bytes, 1, MPI_DOUBLE, MANAGER_PROC, MANAGER_PROC, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to send num_examined_tape_bytes %zd to rank %d\n", num_examined_tape_bytes, MANAGER_PROC);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}
#endif


void send_manager_regs_buffer(path_item *buffer, int *buffer_count) {
    //sends a chunk of regular files to the manager
    send_path_buffer(MANAGER_PROC, PROCESSCMD, buffer, buffer_count);
}

void send_manager_dirs_buffer(path_item *buffer, int *buffer_count) {
    //sends a chunk of regular files to the manager
    send_path_buffer(MANAGER_PROC, DIRCMD, buffer, buffer_count);
}

#ifdef TAPE
void send_manager_tape_buffer(path_item *buffer, int *buffer_count) {
    //sends a chunk of regular files to the manager
    send_path_buffer(MANAGER_PROC, TAPECMD, buffer, buffer_count);
}
#endif

void send_manager_new_buffer(path_item *buffer, int *buffer_count) {
    //send manager new inputs
    send_path_buffer(MANAGER_PROC, INPUTCMD, buffer, buffer_count);
}

void send_manager_work_done() {
    //the worker is finished processing, notify the manager
    send_command(MANAGER_PROC, WORKDONECMD);
}

//worker
void update_chunk(path_item *buffer, int *buffer_count) {
    send_path_buffer(ACCUM_PROC, UPDCHUNKCMD, buffer, buffer_count);
}

void write_output(char *message, int log) {
    //write a single line using the outputproc
    //set the command type
    if (log == 0) {
        send_command(OUTPUT_PROC, OUTCMD);
    }
    else if (log == 1) {
        send_command(OUTPUT_PROC, LOGCMD);
    }
    //send the message
    if (MPI_Send(message, MESSAGESIZE, MPI_CHAR, OUTPUT_PROC, OUTPUT_PROC, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to message to rank %d\n", OUTPUT_PROC);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}


void write_buffer_output(char *buffer, int buffer_size, int buffer_count) {
    //write a buffer to the output proc
    //set the command type
    send_command(OUTPUT_PROC, BUFFEROUTCMD);
    //send the size of the buffer
    if (MPI_Send(&buffer_count, 1, MPI_INT, OUTPUT_PROC, OUTPUT_PROC, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to buffer_count %d to rank %d\n", buffer_count, OUTPUT_PROC);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    if (MPI_Send(buffer, buffer_size, MPI_PACKED, OUTPUT_PROC, OUTPUT_PROC, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to message to rank %d\n", OUTPUT_PROC);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}

void send_worker_queue_count(int target_rank, int queue_count) {
    if (MPI_Send(&queue_count, 1, MPI_INT, target_rank, target_rank, MPI_COMM_WORLD) != MPI_SUCCESS) {
        fprintf(stderr, "Failed to queue_count %d to rank %d\n", queue_count, target_rank);
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}

void send_worker_readdir(int target_rank, work_buf_list  **workbuflist, int *workbufsize) {
    //send a worker a buffer list of paths to stat
    send_buffer_list(target_rank, DIRCMD, workbuflist, workbufsize);
}

#ifdef TAPE
void send_worker_tape_path(int target_rank, work_buf_list  **workbuflist, int *workbufsize) {
    //send a worker a buffer list of paths to stat
    send_buffer_list(target_rank, TAPECMD, workbuflist, workbufsize);
}
#endif

void send_worker_copy_path(int target_rank, work_buf_list  **workbuflist, int *workbufsize) {
    //send a worker a list buffers with paths to copy
    send_buffer_list(target_rank, COPYCMD, workbuflist, workbufsize);
}

void send_worker_compare_path(int target_rank, work_buf_list  **workbuflist, int *workbufsize) {
    //send a worker a list buffers with paths to compare
    send_buffer_list(target_rank, COMPARECMD, workbuflist, workbufsize);
}

void send_worker_exit(int target_rank) {
    //order a rank to exit
    send_command(target_rank, EXITCMD);
}


//functions that use workers
void errsend(int fatal, char *error_text) {
    //send an error message to the outputproc. Die if fatal.
    char errormsg[MESSAGESIZE];
    if (fatal) {
        snprintf(errormsg, MESSAGESIZE, "ERROR FATAL: %s\n",error_text);
    }
    else {
        snprintf(errormsg, MESSAGESIZE, "ERROR NONFATAL: %s\n",error_text);
    }
    write_output(errormsg, 1);
    if (fatal) {
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
    else {
        send_manager_nonfatal_inc();
    }
}

#ifdef FUSE_CHUNKER
int is_fuse_chunk(const char *path, struct options o) {
  if (path && strstr(path, o.fuse_path)){
    return 1;
  } 
  else{
    return 0;
  }
}

void set_fuse_chunk_data(path_item *work_node) {
    int i;
    int numchars;
    char linkname[PATHSIZE_PLUS], baselinkname[PATHSIZE_PLUS];
    const char delimiters[] =  ".";
    char *current;
    char errormsg[MESSAGESIZE];
    size_t length;
    memset(linkname,'\0', sizeof(PATHSIZE_PLUS));
    numchars = readlink(work_node->path, linkname, PATHSIZE_PLUS);
    if (numchars < 0) {
        sprintf(errormsg, "Failed to read link %s", work_node->path);
        errsend(NONFATAL, errormsg);
        return;
    }
    linkname[numchars] = '\0';
    strncpy(baselinkname, basename(linkname), PATHSIZE_PLUS);
    current = strdup(baselinkname);
    strtok(current, delimiters);
    for (i = 0; i < 2; i++) {
        strtok(NULL, delimiters);
    }
    length = atoll(strtok(NULL, delimiters));
    work_node->chkidx = 0;
    work_node->chksz = length;
}

int get_fuse_chunk_attr(const char *path, off_t offset, size_t length, struct utimbuf *ut, uid_t *userid, gid_t *groupid) {
    char value[10000];
    int valueLen = 0;
    char chunk_name[50];
    int chunk_num = 0;
    if (length == 0) {
        return -1;
    }
    chunk_num = offset/length;
    snprintf(chunk_name, 50, "user.chunk_%d", chunk_num);
#ifdef __APPLE__
    valueLen = getxattr(path, chunk_name, value, 10000, 0, 0);
#else
    valueLen = getxattr(path, chunk_name, value, 10000);
#endif
    if (valueLen != -1) {
        sscanf(value, "%10lld %10lld %8d %8d", (long long int *) &(ut->actime), (long long int *) &(ut->modtime), userid, groupid);
    }
    else {
        return -1;
    }
    return 0;
}

int set_fuse_chunk_attr(const char *path, off_t offset, size_t length, struct utimbuf ut, uid_t userid, gid_t groupid) {
    char value[10000];
    int valueLen = 0;
    char chunk_name[50];
    int chunk_num = 0;
    chunk_num = offset/length;
    snprintf(chunk_name, 50, "user.chunk_%d", chunk_num);
    sprintf(value, "%lld %lld %d %d", (long long int) ut.actime, (long long int ) ut.modtime, userid, groupid);
#ifdef __APPLE__
    valueLen = setxattr(path, chunk_name, value, 10000, 0, XATTR_CREATE);
#else
    valueLen = setxattr(path, chunk_name, value, 10000, XATTR_CREATE);
#endif
    if (valueLen != -1) {
        return 0;
    }
    else {
        return -1;
    }
}
#endif


void get_stat_fs_info(const char *path, int *fs) {
#ifdef HAVE_SYS_VFS_H
    struct stat st;
    struct statfs stfs;
    char errortext[MESSAGESIZE];
    int rc;
    char use_path[PATHSIZE_PLUS];
    strncpy(use_path, path, PATHSIZE_PLUS);
    rc = lstat(use_path, &st);
    if (rc < 0) {
        strcpy(use_path, dirname(strdup(path)));
        rc = lstat(use_path, &st);
        if (rc < 0) {
            fprintf(stderr, "Failed to stat path %s\n", path);
            MPI_Abort(MPI_COMM_WORLD, -1);
        }
    }
    if (!S_ISLNK(st.st_mode)) {
        if (statfs(use_path, &stfs) < 0) {
            snprintf(errortext, MESSAGESIZE, "Failed to statfs path %s", path);
            errsend(FATAL, errortext);
        }
        if (stfs.f_type == GPFS_FILE) {
            *fs = GPFSFS;
        }
        else if (stfs.f_type == PANFS_FILE) {
            *fs = PANASASFS;
        }
#ifdef FUSE_CHUNKER
        else if (stfs.f_type == FUSE_SUPER_MAGIC) {
            //fuse file
            *fs = GPFSFS;
        }
#endif
        else {
            *fs = ANYFS;
        }
    }
    else {
        //symlink
        *fs = GPFSFS;
    }
#else
    *fs = ANYFS;
#endif
}

int get_free_rank(int *proc_status, int start_range, int end_range) {
    //given an inclusive range, return the first encountered free rank
    int i;
    for (i = start_range; i <= end_range; i++) {
        if (proc_status[i] == 0) {
            return i;
        }
    }
    return -1;
}

int processing_complete(int *proc_status, int nproc) {
    //are all the ranks free?
    int i;
    int count = 0;
    for (i = 0; i < nproc; i++) {
        if (proc_status[i] == 1) {
            count++;
        }
    }
    return count;
}

//Queue Function Definitions
void enqueue_path(path_list **head, path_list **tail, char *path, int *count) {
    //stick a path on the end of the queue
    path_list *new_node = malloc(sizeof(path_list));
    strncpy(new_node->data.path, path, PATHSIZE_PLUS);
    new_node->next = NULL;
    if (*head == NULL) {
        *head = new_node;
        *tail = *head;
    }
    else {
        (*tail)->next = new_node;
        *tail = (*tail)->next;
    }
    /*
      while (temp_node->next != NULL){
        temp_node = temp_node->next;
      }
      temp_node->next = new_node;
    }*/
    *count += 1;
}


void print_queue_path(path_list *head) {
    //print the entire queue
    while(head != NULL) {
        printf("%s\n", head->data.path);
        head = head->next;
    }
}

void delete_queue_path(path_list **head, int *count) {
    //delete the entire queue;
    path_list *temp = *head;
    while(temp) {
        *head = (*head)->next;
        free(temp);
        temp = *head;
    }
    *count = 0;
}

void enqueue_node(path_list **head, path_list **tail, path_list *new_node, int *count) {
    //enqueue a node using an existing node (does a new allocate, but allows us to pass nodes instead of paths)
    path_list *temp_node = malloc(sizeof(path_list));
    temp_node->data = new_node->data;
    temp_node->next = NULL;
    if (*head == NULL) {
        *head = temp_node;
        *tail = *head;
    }
    else {
        (*tail)->next = temp_node;
        *tail = (*tail)->next;
    }
    *count += 1;
}


void dequeue_node(path_list **head, path_list **tail, int *count) {
    //remove a path from the front of the queue
    path_list *temp_node = *head;
    if (temp_node == NULL) {
        return;
    }
    *head = temp_node->next;
    free(temp_node);
    *count -= 1;
}


void enqueue_buf_list(work_buf_list **workbuflist, int *workbufsize, char *buffer, int buffer_size) {
    work_buf_list *current_pos = *workbuflist;
    work_buf_list *new_buf_item = malloc(sizeof(work_buf_list));
    if (*workbufsize < 0) {
        *workbufsize = 0;
    }
    new_buf_item->buf = buffer;
    new_buf_item->size = buffer_size;
    new_buf_item->next = NULL;
    if (current_pos == NULL) {
        *workbuflist = new_buf_item;
        (*workbufsize)++;
        return;
    }
    while (current_pos->next != NULL) {
        current_pos = current_pos->next;
    }
    current_pos->next = new_buf_item;
    (*workbufsize)++;
}

void dequeue_buf_list(work_buf_list **workbuflist, int *workbufsize) {
    work_buf_list *current_pos;
    if (*workbuflist == NULL) {
        return;
    }
    current_pos = (*workbuflist)->next;
    free((*workbuflist)->buf);
    free(*workbuflist);
    *workbuflist = current_pos;
    (*workbufsize)--;
}

void delete_buf_list(work_buf_list **workbuflist, int *workbufsize) {
    while (*workbuflist) {
        dequeue_buf_list(workbuflist, workbufsize);
    }
    *workbufsize = 0;
}

void pack_list(path_list *head, int count, work_buf_list **workbuflist, int *workbufsize) {
    path_list *iter = head;
    int position;
    char *buffer;
    int buffer_size = 0;
    int worksize;
    worksize = sizeof(path_item) * MESSAGEBUFFER;
    buffer = (char *)malloc(worksize);
    position = 0;
    while (iter != NULL) {
        MPI_Pack(&iter->data, sizeof(path_item), MPI_CHAR, buffer, worksize, &position, MPI_COMM_WORLD);
        iter = iter->next;
        buffer_size++;
        if (buffer_size % MESSAGEBUFFER == 0) {
            enqueue_buf_list(workbuflist, workbufsize, buffer, buffer_size);
            buffer_size = 0;
            buffer = (char *)malloc(worksize);
        }
    }
    enqueue_buf_list(workbuflist, workbufsize, buffer, buffer_size);
}


#ifdef THREADS_ONLY
//custom MPI calls
int MPY_Pack(void *inbuf, int incount, MPI_Datatype datatype, void *outbuf, int outcount, int *position, MPI_Comm comm) {
    // check to make sure there is space in the output buffer position+incount <= outcount
    if (*position+incount > outcount) {
        return -1;
    }
    // copy inbuf to outbuf
    bcopy(inbuf,outbuf+(*position),incount);
    // increment position  position=position+incount
    *position=*position+incount;
    return 0;
}

int MPY_Unpack(void *inbuf, int insize, int *position, void *outbuf, int outcount, MPI_Datatype datatype, MPI_Comm comm) {
    // check to make sure there is space in the output buffer position+insize <= outcount
    if ((*position)+outcount > insize)  {
        return -1;
    }
    // copy inbuf to outbuf
    bcopy(inbuf+(*position),outbuf,outcount);
    // increment position  position=position+insize
    *position=*position+outcount;
    return 0;
}


int MPY_Abort(MPI_Comm comm, int errorcode) {
    int i = 0;
    MPII_Member *member;
    for (i = 0; i < comm->group->size; i++) {
        member = (MPII_Member *)(comm->group->members)[i];
        unlock(member->mutex);
        delete_mutex(member->mutex);
    }
    for (i = 0; i < comm->group->size; i++) {
        pthread_kill(pthread_self(), SIGTERM);
    }
    return -1;
}
#endif
