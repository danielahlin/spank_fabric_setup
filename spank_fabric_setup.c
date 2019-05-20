/*****************************************************************************
 *
 * Copyright (c) 2017 Kungliga Tekniska Högskolan 
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 ****************************************************************************
 * Four options are added for salloc, sbatch, and srun: 
 *
 * --adhoc-fabric-partition=interfaceID0:TAG0,interfaceID0:TAG1,interfaceID1:TAG2... where
 *     interfaceID is a site-dependent interface or fabric identifier
 *     (or combination thereof if there are several interfaces on the
 *     sam) and TAG is user-supplied string for the created
 *     partition. The different TAGS should probably be different but
 *     this is not required and depends on the site-adaptions for
 *     interface bringup. The site adaptions may also impose certain
 *     forms on TAG (such as not containing spaces, not look as an
 *     ip-address etc...)Form of the interfaceID is not mandated but
 *     must be consistent with site-specific adaptions in the
 *     associated pre- and post-scripts. Sugestions for interface ID
 *     are:
 *
 *     - interface names such as eth0, ib0 etc (this assumes consistent interface
 *       naming accross the nodes)
 *  
 *     - Fabric identifiers such as the IB subnet prefix
 *
 *     - Indirect names (with per node local name to interface resolution
 *       using site-adaptations in the helper-scripts)
 *
 *
 * --join-fabric-partition=interfaceID:TAG0:pkey1,interfaceID:TAG1:pkey2,... where
 *     pkey1 and pkey2 are pre-existing partitions which the user is
 *     permitted to join.
 *
 *
 * --adhoc-fabric-bind-mpi=interfaceID:TAG try to bind MPI to use the
 *     interface created the fabrice where interfaceID is connected using TAG
 *
 * --isolated-fabric convenience argument equivalent to 
 *     --adhoc-fabric-partition=FABRIC_DEFAULT_INTERFACEID:TAG and
 *     --adhoc-fabric-bind-mpi=FABRIC_DEFAULT_INTERFACEID:TAG
 *
 *   where FABRIC_DEFAULT_INTERFACEID is taken from the configuration
 *   file of this module.
 *
 *
 * Permission and argument are checked by an external program in an
 * advisory mode during the run of the salloc, sbatch, and
 * srun. Permissions are checked in an enforcing mode during the
 * job-setup in the slurmctld prolog.
 *
 ****************************************************************************
 *
 */


#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include <slurm/spank.h>

#define FABRIC_PARTITION_ISOLATED 1
#define FABRIC_PARTITION_CREATE_ADHOC 2
#define FABRIC_PARTITION_JOIN 3
#define FABRIC_PARTITION_BIND_MPI 4
#define FABRIC_PARTITION_DEFAULTIFSTR "FABRIC_DEFAULT_INTERFACEID"

static char* spank_fabric_partition_create_adhoc_env = "SPANK_FABRIC_PARTITION_CREATE_ADHOC";
static char* spank_fabric_partition_join_env = "SPANK_FABRIC_PARTITION_JOIN";
static char* spank_fabric_partition_bind_mpi_env = "SPANK_FABRIC_BIND_MPI";
	    
/*
 * All spank plugins must define this macro for the SLURM plugin loader.
 */
SPANK_PLUGIN(fabric_partition, 1)

static int spank_fabric_partition_opt_process (int val, const char *optarg, int remote);

int   spank_fabric_partition_isolated = -1;
char* spank_fabric_partition_create_adhoc = NULL;
char* spank_fabric_partition_join = NULL;
char* spank_fabric_partition_bind_mpi = NULL;
char* spank_fabric_partition_checker = NULL;
char* spank_fabric_partition_mpi_env_list = NULL;

/*
 *  Provide option for srun:
 */
struct spank_option spank_fabric_partition_option_array[] =
{
     	{ "isolated-fabric", NULL, "Sets up an isolated fabric for this job",
	  0, FABRIC_PARTITION_ISOLATED, (spank_opt_cb_f) spank_fabric_partition_opt_process },
	{ "adhoc-fabric-partitions", "fabricID0:TAG0,fabricID1:TAG1,...", "Setup one or several ad-hoc fabric partitions for the nodes of the job",
	  1, FABRIC_PARTITION_CREATE_ADHOC, (spank_opt_cb_f) spank_fabric_partition_opt_process },
	{ "join-fabric-partitions", "fabricID0:TAG0:pkey1,fabricID1:TAG1:pkey2,...", "Join the nodes of the job to one or several existing fabric partitions",
	  1, FABRIC_PARTITION_JOIN, (spank_opt_cb_f) spank_fabric_partition_opt_process },
	{ "adhoc-fabric-bind-mpi", "interfaceID:TAG,...", "Try to bind MPI to use one or several of the interfaces created",
	  1, FABRIC_PARTITION_BIND_MPI, (spank_opt_cb_f) spank_fabric_partition_opt_process },
	SPANK_OPTIONS_TABLE_END
};


/**
 * Initialization function - parses arguments to the dynamic library and adds parameters to context where it is applicable.
 */
int slurm_spank_init(spank_t sp, int ac, char **av)
{
     int res = ESPANK_SUCCESS;
     int i, c;
      
     // Process arguments  - we need to know:
     // - where the advisory permission check script resides
     // - a file describing which environment variables to set to reflect use of the partitions by various MPI implementations
     optind = 1;
     while ((c = getopt (ac, av, "e:m:")) != -1) {	  
	  switch (c) {
          case 'e':
               if ((spank_fabric_partition_checker = strdup(optarg)) == NULL) {
		    slurm_error("Unable allocate memory for arguments: %s", strerror(errno));
		    res = ESPANK_ERROR;
	       }
               break;
	  case 'm':
               if ((spank_fabric_partition_mpi_env_list = strdup(optarg)) == NULL) {
		    slurm_error("Unable allocate memory for arguments: %s", strerror(errno));
		    res = ESPANK_ERROR;
	       }
               break;
          default:
	       slurm_error("Unable to parse command line");
	       res = ESPANK_ERROR;
	  }
     }
     
     if (spank_fabric_partition_checker == NULL || 
	 spank_fabric_partition_mpi_env_list == NULL) {
	  slurm_error("Insufficient arguments - fabric settings checker and mpi environment list file");
	  return ESPANK_ERROR;
     }
     
     // Options only valid for salloc/sbatch and srun (local context)
     spank_context_t ctx = spank_context();
     if (ctx == S_CTX_ALLOCATOR || ctx ==  S_CTX_LOCAL) {
	  for (i = 0; spank_fabric_partition_option_array[i].name; i++) {
	       res = spank_option_register(sp, &spank_fabric_partition_option_array[i]);
	       if (res != ESPANK_SUCCESS) {
		    slurm_error("Could not register Spank option %s",
				spank_fabric_partition_option_array[i].name);
		    break;
	       }
	  }
     }
     
     return res;
}

/**
 * Called from start of step
 *
 * Set the MPI BINDING environment variables if available.
 */
int slurm_spank_task_init(spank_t sp, int ac, char **av)
{
     int res = ESPANK_SUCCESS;
     int uid;
     int jid;
     int stepid;
     int taskid;
     
     spank_context_t ctx =spank_context();
     if (ctx == S_CTX_REMOTE) {
	  const size_t n = 16384;
	  char buf[n];
	  res = spank_getenv(sp, "SPANK_FABRIC_PARTITIONS_MPI_BIND", buf, n);
	  if (res != ESPANK_SUCCESS) {
	       // Go through packaged variables and set them:
	       char* key_start=buf;
	       char *equal_pos, *pipe_pos;
	       int last = 0;
	       while (last != 1 && key_start < (buf + n) && *key_start != '\0') {
                    // Find position of equal ('=')
		    for (equal_pos = key_start; equal_pos < (buf + n) && *equal_pos != '\0' && *equal_pos != '='; equal_pos++);
		    if (equal_pos >= (buf + n) || *equal_pos == '\0') {
			 // Improperly formed environment variable
			 return ESPANK_BAD_ARG;
		    }
		    // Terminate variable-string there:
		    *equal_pos = '\0';
		    // Find position of pipe ('|') or end of string.
		    for (pipe_pos = equal_pos; pipe_pos < (buf + n) && *pipe_pos != '\0' && *pipe_pos != '|'; pipe_pos++);
		    if (pipe_pos >= (buf + n)) {
			 // Improperly formed environment variable (found no terminating '\0')
			 return ESPANK_BAD_ARG;
		    }
		    // Otherwise we have a key value pair, register it:
		    // If this was the end of string save that knowledge
		    last = (*pipe_pos == '\0');
		    *pipe_pos = '\0';
		    
		    // Register the variable
		    res = spank_setenv(sp, key_start, equal_pos+1, 1);
		    if (res == ESPANK_SUCCESS) {
			 return res;
		    }
		    key_start=pipe_pos + 1;
	       }
	  } else {
	       // Log error message here
	       return res;
	  }
	  return res;
     }
}


/*
 *  Called from both srun and slurmd.
 */
int slurm_spank_init_post_opt (spank_t sp, int ac, char **av)
{
	int res = ESPANK_SUCCESS;
	spank_context_t ctx =spank_context();

	// Options only valid for salloc/sbatch and srun (local
	// context) Note that srun can be called in two different
	// settings - job setting and non-job-setting. In the
	// job-setting case we do not want to take any action since
	// setup should already have been performed during the jobs
	// allocation phase.
	if (ctx == S_CTX_ALLOCATOR || (ctx ==  S_CTX_LOCAL && getenv("SLURM_JOB_ID") == NULL)) {
	     int run_check = 0;
	     if (spank_fabric_partition_isolated != -1) {
		  slurm_debug("spank_fabric_partition_isolated is requested");
		  if (spank_fabric_partition_create_adhoc != NULL || spank_fabric_partition_bind_mpi != NULL) {
		       slurm_error("isolated-fabric can not be requested together with adhoc-fabric-partitions or adhoc-fabric-bind-mpi since it is a shorthand for specific settings of these options ");
		  }
		  spank_fabric_partition_create_adhoc = FABRIC_PARTITION_DEFAULTIFSTR ":isolated";
		  spank_fabric_partition_bind_mpi = FABRIC_PARTITION_DEFAULTIFSTR ":isolated";
		  run_check = 1;
	     }

	     if (spank_fabric_partition_create_adhoc != NULL) {
		  slurm_debug("spank_fabric_partition_create_adhoc is requested");
		  res = spank_job_control_setenv(sp, spank_fabric_partition_create_adhoc_env, spank_fabric_partition_create_adhoc, 1);
		  if (res != ESPANK_SUCCESS) {
		       slurm_error("Unable to set environment variable for adhoc-fabric-partitions: %s", spank_strerror(res));
		  } else {
		       run_check = 1;
		  }
	     }
	     if (spank_fabric_partition_join != NULL) {
		  slurm_debug("spank_fabric_partition_join is requested");
		  res = spank_job_control_setenv(sp, spank_fabric_partition_join_env, spank_fabric_partition_join, 1);
		  if (res != ESPANK_SUCCESS) {
		       slurm_error("Unable to set environment variable for join-fabric-partitions: %s", spank_strerror(res));
		  } else {
		       run_check = 1;
		  }
	     }
	     if (spank_fabric_partition_bind_mpi != NULL) {
		  slurm_debug("spank_fabric_partition_bind_mpi is requested");
		  res = spank_job_control_setenv(sp, spank_fabric_partition_bind_mpi_env, spank_fabric_partition_bind_mpi, 1);
		  if (res != ESPANK_SUCCESS) {
		       slurm_error("Unable to set environment variable for adhoc-fabric-bind-mpi: %s", spank_strerror(res));
		  } else {
		       run_check = 1;
		  }
	     }


	     // If any option was correctly enabled let an external (locally adapted) program perform further checking of the partition arguments:
	     if (run_check) {
		  char cmd[1024];
		  snprintf(cmd, 1024*sizeof(cmd), "%s "
			   "\"--spank_fabric_partition_create_adhoc=%s\" "
			   "\"--spank_fabric_partition_join=%s\" " 
			   "\"--spank_fabric_partition_bind_mpi=%s\" ",
			   spank_fabric_partition_checker,
			   spank_fabric_partition_create_adhoc != NULL ? spank_fabric_partition_create_adhoc : "",
			   spank_fabric_partition_join != NULL ? spank_fabric_partition_join : "",
			   spank_fabric_partition_bind_mpi != NULL ? spank_fabric_partition_bind_mpi : "" 			   
		       );
		  res = system(cmd);
		  if (res < 0) {
		       slurm_error("Failed to execute partition argument checker: %s", strerror(errno));
		       res = ESPANK_ERROR;
		  } else if (res > 0) {
		       slurm_error("Check of partition arguments failed: %s", cmd);
		       res = ESPANK_ERROR;
		  } else {
		       res = ESPANK_SUCCESS;
		  }
	     }
	}
	     
	return res;
}

/**
 * Callback for options handling
 */
static int spank_fabric_partition_opt_process (int val, const char *optarg, int remote)
{
     switch(val) {
     case FABRIC_PARTITION_ISOLATED:
	  spank_fabric_partition_isolated = 1;
	  break;
     case FABRIC_PARTITION_CREATE_ADHOC:
	  if ((spank_fabric_partition_create_adhoc = strdup(optarg)) == NULL) {
	       return errno;
	  }	  
	  break;
     case FABRIC_PARTITION_JOIN:
	  if ((spank_fabric_partition_join = strdup(optarg)) == NULL) {
	       return errno;	       
	  }	  
	  break;
     case FABRIC_PARTITION_BIND_MPI:
	  if ((spank_fabric_partition_bind_mpi = strdup(optarg)) == NULL) {
	       return errno;
	       
	  }
	  break;
     }		    
     
     return 0;
}

