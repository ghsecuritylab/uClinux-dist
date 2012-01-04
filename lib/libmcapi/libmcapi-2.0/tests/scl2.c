/* Test: scl1
   Description: Tests scl_channel_send and scl_channel_recv calls on single node.  
   Note, that for scalar channels we only have blocking versions of send/recv.
   This test tests send/recv to several endpoints on the same node.  It tests
   all error conditions.  
*/

#include <mcapi.h>
#include <mca.h>
#include <mcapi_test.h>
#include <stdio.h>
#include <stdlib.h> /* for malloc */
#include <string.h>
#include <mcapi_impl_spec.h>

#include <sys/wait.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NUM_SIZES 4
#define BUFF_SIZE 64
#define DOMAIN 0
#define NODE 0


#define WRONG wrong(__LINE__);
void wrong(unsigned line)
{
  fprintf(stderr,"WRONG: line=%u\n", line);
  fflush(stdout);
  _exit(1);
}

mcapi_boolean_t send (mcapi_sclchan_send_hndl_t send_handle, mcapi_endpoint_t recv,unsigned long long data,uint32_t size,mcapi_status_t status,int exp_status) {
  mcapi_boolean_t rc = MCAPI_FALSE;
  switch (size) {
  case (8): mcapi_sclchan_send_uint8(send_handle,data,&status); break;
  case (16): mcapi_sclchan_send_uint16(send_handle,data,&status); break;
  case (32): mcapi_sclchan_send_uint32(send_handle,data,&status); break;
  case (64): mcapi_sclchan_send_uint64(send_handle,data,&status); break;
  default: fprintf (stderr,"ERROR: bad data size in call to send\n");
  };
  if (status == MCAPI_SUCCESS) {
    fprintf(stderr,"endpoint=%i has sent %i byte(s): [%llx]\n",(int)send_handle,(int)size/8,data);
  }
  if (status == exp_status) {
    rc = MCAPI_TRUE;
  }
  return rc;
}

mcapi_boolean_t recv (mcapi_sclchan_recv_hndl_t recv_handle,uint32_t size,mcapi_status_t status,uint64_t exp_status,unsigned long long exp_data) {
  unsigned long long data = 0;
  mcapi_boolean_t rc = MCAPI_FALSE;
  uint64_t size_mask; 
  switch (size) {
  case (8): size_mask = 0xff;data=mcapi_sclchan_recv_uint8(recv_handle,&status); break;
  case (16): size_mask = 0xffff;data=mcapi_sclchan_recv_uint16(recv_handle,&status); break;
  case (32): size_mask = 0xffffffff;data=mcapi_sclchan_recv_uint32(recv_handle,&status); break;
  case (64): size_mask = 0xffffffffffffffffULL;data=mcapi_sclchan_recv_uint64(recv_handle,&status); break;
  default: fprintf (stderr,"ERROR: bad data size in call to send\n");
  };
 
  exp_data = exp_data & size_mask;
   
  if (status == exp_status) {
    rc = MCAPI_TRUE;
  }
  if (status == MCAPI_SUCCESS) {
	  fprintf(stderr,"endpoint=%i has received %i byte(s): [%llx]!!!\n",(int)recv_handle,(int)size/8,data);
	  if (data != exp_data) { 
		  fprintf(stderr, "expected %llx, received %llxd\n",exp_data,data); 
		  rc = MCAPI_FALSE; 
	  }
  }

  return rc;
}

void do_child()
{
	mcapi_status_t status;
	mcapi_param_t parms;
	mcapi_info_t version;
	mcapi_request_t request;
	mcapi_endpoint_t ep1,ep2,ep3,ep4;
	mcapi_sclchan_send_hndl_t s1;
	mcapi_boolean_t rc = MCAPI_FALSE;

	mcapi_uint_t avail;
	int s;
	int i;
	int sizes[NUM_SIZES] = {8,16,32,64};
	uint64_t test_pattern = 0x9282726252423222ULL;
	size_t size;
	int fd;

	fd = open("/tmp/child.log", O_CREAT | O_RDWR, 0666);
	if (fd < 0)
		{ WRONG };

	if (dup2(fd, STDOUT_FILENO) == -1) {
		WRONG;
	}
	close(fd);

	printf("test log\n");

	/* create a node */
	mcapi_initialize(DOMAIN,NODE,NULL,&parms,&version,&status);
	if (status != MCAPI_SUCCESS) { WRONG }

	ep1 = mcapi_endpoint_create(MASTER_PORT_NUM2,&status);
	if (status != MCAPI_SUCCESS) { WRONG }
	printf("ep2 %x   \n", ep2);

	ep3 = mcapi_endpoint_get(DOMAIN,SLAVE_NODE_NUM, SLAVE_PORT_NUM2,MCA_INFINITE, &status);
	if (status != MCAPI_SUCCESS) { WRONG }

	mcapi_sclchan_connect_i(ep1,ep3,&request, &status);
	if (status != MCAPI_SUCCESS) { WRONG }

	/*************************** open the channels *********************/
	mcapi_sclchan_send_open_i(&s1 /*send_handle*/,ep1, &request, &status);
	if (status != MCAPI_SUCCESS) { WRONG }

	sleep(1);
	/* test send/recv of different sizes */
	for (s = 0; s < NUM_SIZES; s++) {
		size = sizes[s];
		/* send and recv messages on the channels */
		/* regular endpoints */
		rc = send (s1,ep3,test_pattern,size,status,MCAPI_SUCCESS);
		if (!rc) {WRONG}
	}

	mcapi_sclchan_send_close_i(s1,&request,&status);

	mcapi_endpoint_delete(ep1,&status);
	if (status != MCAPI_SUCCESS) { WRONG }


	mcapi_finalize(&status);

	printf("Child Test PASSED\n");

	close(STDOUT_FILENO);
        _exit(0);
}

void do_parent(int pid)
{
	mcapi_status_t status;
	mcapi_param_t parms;
	mcapi_info_t version;
	mcapi_request_t request;
	mcapi_endpoint_t ep1,ep2,ep3,ep4;
	mcapi_sclchan_send_hndl_t s1;
	mcapi_sclchan_recv_hndl_t r1;
	mcapi_boolean_t rc = MCAPI_FALSE;

	mcapi_uint_t avail;
	int stat_val;
	int s,pass_num=0;
	int i;
	int sizes[NUM_SIZES] = {8,16,32,64};
	uint64_t test_pattern = 0x9383736353433323ULL;
	size_t size;
	int fd;

	fd = open("/tmp/parent.log", O_CREAT | O_RDWR, 0666);
	if (fd < 0)
		{ WRONG };

	if (dup2(fd, STDOUT_FILENO) == -1) {
		WRONG;
	}
	close(fd);

	/* create a node */
	mcapi_initialize(DOMAIN,NODE,NULL,&parms,&version,&status);
	if (status != MCAPI_SUCCESS) { WRONG }

	ep2 = mcapi_endpoint_create(MASTER_PORT_NUM1,&status);
	if (status != MCAPI_SUCCESS) { WRONG }
	printf("ep2 %x   \n", ep2);

	ep4 = mcapi_endpoint_get(DOMAIN,SLAVE_NODE_NUM, SLAVE_PORT_NUM1,MCA_INFINITE, &status);
	if (status != MCAPI_SUCCESS) { WRONG }

	mcapi_sclchan_connect_i(ep2,ep4,&request, &status);
	if (status != MCAPI_SUCCESS) { WRONG }

	/*************************** open the channels *********************/
	mcapi_sclchan_send_open_i(&s1 /*send_handle*/,ep2, &request, &status);
	if (status != MCAPI_SUCCESS) { WRONG }

	sleep(1);
	/* test send/recv of different sizes */
	for (s = 0; s < NUM_SIZES; s++) {
		size = sizes[s];
		/* send and recv messages on the channels */
		/* regular endpoints */
		rc = send (s1,ep4,test_pattern,size,status,MCAPI_SUCCESS);
		if (!rc) {WRONG}
	}

	mcapi_sclchan_send_close_i(s1,&request,&status);

# if 1 
	mcapi_sclchan_recv_open_i(&r1 /*recv_handle*/,ep2, &request, &status);

	while (1) {
		avail = mcapi_sclchan_available(r1, &status);
		if (avail > 0) {
			test_pattern = 0x1122334455667788ULL;
	                rc = recv(r1,64,status, MCAPI_SUCCESS, test_pattern);
			if (status != MCAPI_TRUE) { WRONG }
			else {pass_num++;}
			break;
		}
		sleep(2);
	}
	mcapi_sclchan_recv_close_i(r1,&request,&status); 

#endif
	printf("Parent  wait child process\n");
        waitpid(pid, &stat_val, 0);
	if (WIFEXITED(stat_val))
		 printf("Child exited with code %d\n",WEXITSTATUS(stat_val));
	else if (WIFSIGNALED(stat_val))
		printf("Child terminated  abnormally, signal %d\n", WTERMSIG(stat_val));

        mcapi_endpoint_delete(ep2,&status);
        if (status != MCAPI_SUCCESS) { WRONG }

	mcapi_finalize(&status);

	if (pass_num == 1) {
    	printf("Parent Test PASSED\n");
	fflush(stdout);
  	} else {
    	printf("Parent Test FAILED\n");
	fflush(stdout);
  	}
}


int main (int ac, char **av) {
	mcapi_status_t status;
	mcapi_param_t parms;
	mcapi_info_t version;
	mcapi_request_t request;
	mcapi_endpoint_t ep1,ep2,ep3,ep4;

	/* cases:
	1: both named endpoints (1,2)
	*/
	mcapi_sclchan_send_hndl_t s1;
	mcapi_sclchan_recv_hndl_t r1;
	mcapi_uint_t avail;
	int s;
	int i;
	int sizes[NUM_SIZES] = {8,16,32,64};
	uint64_t test_pattern = 0x1122334455667788ULL;
	size_t size;
	mcapi_boolean_t rc = MCAPI_FALSE;

	int parentpid, childpid;
	char opt;

	while ( (opt = getopt(ac, av, "C")) > 0) {

 	switch (opt) {
                case 'C': /* Run child */
			do_child();
                        break;
                default:
			break;

 		}
	}

	childpid = vfork();
	if (childpid < 0) {
		WRONG
	}
	if (childpid == 0) {/* child */

        return	execlp(av[0], av[0], "-C", (char *) NULL);

	} else {/* parent */
 

	do_parent(childpid);	

        }
    	printf("CoreA Test PASSED\n");
	fflush(stdout);
	close(STDOUT_FILENO);
	return 0;
}
