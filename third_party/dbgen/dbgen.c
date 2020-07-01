/* main driver for dss banchmark */

#define DECLARER				/* EXTERN references get defined here */
#define NO_FUNC (int (*) ()) NULL	/* to clean up tdefs */
#define NO_LFUNC (long (*) ()) NULL		/* to clean up tdefs */

#include "config.h"
#include "release.h"
#include <stdlib.h>
#if (defined(_POSIX_)||!defined(WIN32))		/* Change for Windows NT */
#include <unistd.h>
#include <sys/wait.h>
#endif /* WIN32 */
#include <stdio.h>				/* */
#include <limits.h>
#include <math.h>
#include <ctype.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#ifdef HP
#include <strings.h>
#endif
#if (defined(WIN32)&&!defined(_POSIX_))
#include <process.h>
#pragma warning(disable:4201)
#pragma warning(disable:4214)
#pragma warning(disable:4514)
#define WIN32_LEAN_AND_MEAN
#define NOATOM
#define NOGDICAPMASKS
#define NOMETAFILE
#define NOMINMAX
#define NOMSG
#define NOOPENFILE
#define NORASTEROPS
#define NOSCROLL
#define NOSOUND
#define NOSYSMETRICS
#define NOTEXTMETRIC
#define NOWH
#define NOCOMM
#define NOKANJI
#define NOMCX
#include <windows.h>
#pragma warning(default:4201)
#pragma warning(default:4214)
#endif

#include "dss.h"
#include "dsstypes.h"
#include "dbgen.h"
#define debug(fmt, ...) printf("%s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__);

extern int optind, opterr;
extern char *optarg;
DSS_HUGE rowcnt = 0, minrow = 0;
long upd_num = 0;
double flt_scale;
#if (defined(WIN32)&&!defined(_POSIX_))
char *spawn_args[25];
#endif
static int bTableSet = 0;

extern seed_t Seed[];
seed_t seed_backup[MAX_STREAM + 1];
static bool first_invocation = true;

/*
* general table descriptions. See dss.h for details on structure
* NOTE: tables with no scaling info are scaled according to
* another table
*
*
* the following is based on the tdef structure defined in dss.h as:
* typedef struct
* {
* char     *name;            -- name of the table; 
*                               flat file output in <name>.tbl
* long      base;            -- base scale rowcount of table; 
*                               0 if derived
* int       (*loader) ();    -- function to present output
* long      (*gen_seed) ();  -- functions to seed the RNG
* int       child;           -- non-zero if there is an associated detail table
* unsigned long vtotal;      -- "checksum" total 
* }         tdef;
*
*/

//monetdbe_export char* monetdbe_append(monetdbe_database dbhdl, const char* schema, const char* table, monetdbe_column **input, size_t column_count);
tdef tdefs[] = {
    {"part.tbl", "part table", 200000, NULL, NULL, PSUPP, 0},
    {"partsupp.tbl", "partsupplier table", 200000, NULL, NULL, NONE, 0},
    {"supplier.tbl", "suppliers table", 10000, NULL, NULL, NONE, 0},
    {"customer.tbl", "customers table", 150000, NULL, NULL, NONE, 0},
    {"orders.tbl", "order table", 150000, NULL, NULL, LINE, 0},
    {"lineitem.tbl", "lineitem table", 150000, NULL, NULL, NONE, 0},
    {"orders.tbl", "orders/lineitem tables", 150000, NULL, NULL, LINE, 0},
    {"part.tbl", "part/partsupplier tables", 200000, NULL, NULL, PSUPP, 0},
    {"nation.tbl", "nation table", NATIONS_MAX, NULL, NULL, NONE, 0},
    {"region.tbl", "region table", NATIONS_MAX, NULL, NULL, NONE, 0},
};


/*
* read the distributions needed in the benchamrk
*/
void
load_dists (void)
{
	read_dist (env_config (DIST_TAG, DIST_DFLT), "p_cntr", &p_cntr_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "colors", &colors);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "p_types", &p_types_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "nations", &nations);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "regions", &regions);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "o_oprio",
		&o_priority_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "instruct",
		&l_instruct_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "smode", &l_smode_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "category",
		&l_category_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "rflag", &l_rflag_set);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "msegmnt", &c_mseg_set);

	/* load the distributions that contain text generation */
	read_dist (env_config (DIST_TAG, DIST_DFLT), "nouns", &nouns);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "verbs", &verbs);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "adjectives", &adjectives);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "adverbs", &adverbs);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "auxillaries", &auxillaries);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "terminators", &terminators);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "articles", &articles);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "prepositions", &prepositions);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "grammar", &grammar);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "np", &np);
	read_dist (env_config (DIST_TAG, DIST_DFLT), "vp", &vp);
	
}

typedef struct append_info_t {
    size_t ncols;
    monetdbe_column** cols;
    size_t counter;
} append_info_t;

/*
* Function prototypes
*/
static void	gen_tbl (int tnum,  DSS_HUGE count, append_info_t*);

static void* Zalloc(monetdbe_types t, DSS_HUGE n) {
    switch(t) {
        case monetdbe_bool:
            return malloc(sizeof(bool)*n);
        case monetdbe_int8_t: 
            return malloc(sizeof(int8_t)*n);
        case monetdbe_int16_t: 
            return malloc(sizeof(int16_t)*n);
        case monetdbe_int32_t: 
            return malloc(sizeof(int32_t)*n);
        case monetdbe_int64_t: 
            return malloc(sizeof(int64_t)*n);
        case monetdbe_float: 
            return malloc(sizeof(float)*n);
        case monetdbe_double:
            return malloc(sizeof(double)*n);
        case monetdbe_str: 
            return malloc(sizeof(char**)*n);
        case monetdbe_blob: 
            return malloc(sizeof(monetdbe_data_blob)*n);
        case monetdbe_date: 
            return malloc(sizeof(monetdbe_data_date)*n);
        case monetdbe_time:
            return malloc(sizeof(monetdbe_data_time)*n);
        case monetdbe_timestamp:
            return malloc(sizeof(monetdbe_data_timestamp)*n);
        default:
            return NULL;
    }
}
static void init_info(append_info_t* t, DSS_HUGE count) {
    int _type;
    for(size_t i=0; i < (t->ncols); i++) {
        t->cols[i]->count = count;
        t->cols[i]->data = Zalloc(t->cols[i]->type, count);
    }
}

static void append_region(code_t* c, append_info_t* t) {
    size_t k = t->counter;
    for (size_t i=0; i < (t->ncols); i++) {
         if(strcmp(t->cols[i]->name, "r_regionkey") == 0){
             ((int64_t*)t->cols[i]->data)[k] = c->code; 
         }
         if(strcmp(t->cols[i]->name, "r_name") == 0){
             ((char**)t->cols[i]->data)[k] = c->text; 
         }
         if(strcmp(t->cols[i]->name, "r_comment") == 0){
             ((char**)t->cols[i]->data)[k] = c->comment; 
         }
   }
    t->counter++;
}


//typedef struct {
//    monetdbe_types type;
//    void *data;
//    size_t count;
//    char* name;
//} monetdbe_column;
//
//
//typedef struct
//{
//    DSS_HUGE            code;
//    char            *text;
//    long            join;
//    char            comment[N_CMNT_MAX + 1];
//    int             clen;
//} code_t;

static void gen_tbl(int tnum, DSS_HUGE count, append_info_t* info) {
	order_t o;
	supplier_t supp;
	customer_t cust;
	part_t part;
	code_t code;

	for (DSS_HUGE i = 1; count; count--, i++) {
		row_start(tnum);
		switch (tnum) {
		case LINE:
		case ORDER:
		case ORDER_LINE:
			mk_order(i, &o, 0);
			// append_order_line(&o, info);
			break;
		case SUPP:
			mk_supp(i, &supp);
			// append_supp(&supp, info);
			break;
		case CUST:
			mk_cust(i, &cust);
			// append_cust(&cust, info);
			break;
		case PSUPP:
		case PART:
		case PART_PSUPP:
			mk_part(i, &part);
			// append_part_psupp(&part, info);
			break;
		case NATION:
			mk_nation(i, &code);
			// append_nation(&code, info);
			break;
		case REGION:
			mk_region(i, &code);
			append_region(&code, info);
			break;
		}
		row_stop_h(tnum);
	}
}

char*  get_table_name(int num) {
	switch (num) {
	case PART:
		return "part";
	case PSUPP:
		return "partsupp";
	case SUPP:
		return "supplier";
	case CUST:
		return "customer";
	case ORDER:
		return "orders";
	case LINE:
		return "lineitem";
	case NATION:
		return "nation";
	case REGION:
		return "region";
	default:
		return "";
	}
}

#define REGION_SCHEMA(schema) "CREATE TABLE "schema".region"\
	       " ("\
	       "r_regionkey INT NOT NULL,"\
	       "r_name VARCHAR(25) NOT NULL,"\
	       "r_comment VARCHAR(152) NOT NULL);"

#define NATION_SCHEMA(schema) "CREATE TABLE "schema".nation"\
	       " ("\
	       "n_nationkey INT NOT NULL,"\
	       "n_name VARCHAR(25) NOT NULL,"\
	       "n_regionkey INT NOT NULL,"\
	       "n_comment VARCHAR(152) NOT NULL);"

#define SUPPLIER_SCHEMA(schema) "CREATE TABLE "schema".supplier"\
	       " ("\
	       "s_suppkey INT NOT NULL,"\
	       "s_name VARCHAR(25) NOT NULL,"\
	       "s_address VARCHAR(40) NOT NULL,"\
	       "s_nationkey INT NOT NULL,"\
	       "s_phone VARCHAR(15) NOT NULL,"\
	       "s_acctbal DECIMAL(15,2) NOT NULL,"\
	       "s_comment VARCHAR(101) NOT NULL);"

#define CUSTOMER_SCHEMA(schema) "CREATE TABLE "schema".customer"\
	       " ("\
	       "c_custkey INT NOT NULL,"\
	       "c_name VARCHAR(25) NOT NULL,"\
	       "c_address VARCHAR(40) NOT NULL,"\
	       "c_nationkey INT NOT NULL,"\
	       "c_phone VARCHAR(15) NOT NULL,"\
	       "c_acctbal DECIMAL(15,2) NOT NULL,"\
	       "c_mktsegment VARCHAR(10) NOT NULL,"\
	       "c_comment VARCHAR(117) NOT NULL);"

#define PART_SCHEMA(schema) "CREATE TABLE "schema".part"\
	       " ("\
	       "p_partkey INT NOT NULL,"\
	       "p_name VARCHAR(55) NOT NULL,"\
	       "p_mfgr VARCHAR(25) NOT NULL,"\
	       "p_brand VARCHAR(10) NOT NULL,"\
	       "p_type VARCHAR(25) NOT NULL,"\
	       "p_size INT NOT NULL,"\
	       "p_container VARCHAR(10) NOT NULL,"\
	       "p_retailprice DECIMAL(15,2) NOT NULL,"\
	       "p_comment VARCHAR(23) NOT NULL);"


#define PART_SUPP_SCHEMA(schema) "CREATE TABLE "schema".partsupp"\
	       " ("\
	       "ps_partkey INT NOT NULL,"\
	       "ps_suppkey INT NOT NULL,"\
	       "ps_availqty INT NOT NULL,"\
	       "ps_supplycost DECIMAL(15,2) NOT NULL,"\
	       "ps_comment VARCHAR(199) NOT NULL);"

#define ORDERS_SCHEMA(schema) "CREATE TABLE "schema".orders"\
	       " ("\
	       "o_orderkey INT NOT NULL,"\
	       "o_custkey INT NOT NULL,"\
	       "o_orderstatus VARCHAR(1) NOT NULL,"\
	       "o_totalprice DECIMAL(15,2) NOT NULL,"\
	       "o_orderdate DATE NOT NULL,"\
	       "o_orderpriority VARCHAR(15) NOT NULL,"\
	       "o_clerk VARCHAR(15) NOT NULL,"\
	       "o_shippriority INT NOT NULL,"\
	       "o_comment VARCHAR(79) NOT NULL);"

#define LINE_ITEM_SCHEMA(schema) "CREATE TABLE "schema".lineitem"\
	       " ("\
	       "l_orderkey INT NOT NULL,"\
	       "l_partkey INT NOT NULL,"\
	       "l_suppkey INT NOT NULL,"\
	       "l_linenumber INT NOT NULL,"\
	       "l_quantity INTEGER NOT NULL,"\
	       "l_extendedprice DECIMAL(15,2) NOT NULL,"\
	       "l_discount DECIMAL(15,2) NOT NULL,"\
	       "l_tax DECIMAL(15,2) NOT NULL,"\
	       "l_returnflag VARCHAR(1) NOT NULL,"\
	       "l_linestatus VARCHAR(1) NOT NULL,"\
	       "l_shipdate DATE NOT NULL,"\
	       "l_commitdate DATE NOT NULL,"\
	       "l_receiptdate DATE NOT NULL,"\
	       "l_shipinstruct VARCHAR(25) NOT NULL,"\
	       "l_shipmode VARCHAR(10) NOT NULL,"\
	       "l_comment VARCHAR(44) NOT NULL)"

char* dbgen(double flt_scale, monetdbe_database mdbe, char* schema){
    char* err= NULL;
    if((err=monetdbe_query(mdbe, REGION_SCHEMA("sys"), NULL, NULL)) != NULL)
        return err;
    if((err=monetdbe_query(mdbe, NATION_SCHEMA("sys"), NULL, NULL)) != NULL)
        return err;
    if((err=monetdbe_query(mdbe, SUPPLIER_SCHEMA("sys"), NULL, NULL)) != NULL)
        return err;
    if((err=monetdbe_query(mdbe, CUSTOMER_SCHEMA("sys"), NULL, NULL)) != NULL)
        return err;
    if((err=monetdbe_query(mdbe, PART_SCHEMA("sys"), NULL, NULL)) != NULL)
        return err;
    if((err=monetdbe_query(mdbe, PART_SUPP_SCHEMA("sys"), NULL, NULL)) != NULL)
        return err;
    if((err=monetdbe_query(mdbe, ORDERS_SCHEMA("sys"), NULL, NULL)) != NULL)
        return err;
    if((err=monetdbe_query(mdbe, LINE_ITEM_SCHEMA("sys"), NULL, NULL)) != NULL)
        return err;
	if (flt_scale == 0) {
		// schema only
		return NULL;
	}
    
	// generate the actual data
	DSS_HUGE rowcnt = 0;
	DSS_HUGE i;
	// all tables
	table = (1 << CUST) | (1 << SUPP) | (1 << NATION) | (1 << REGION) | (1 << PART_PSUPP) | (1 << ORDER_LINE);
	force = 0;
	insert_segments = 0;
	delete_segments = 0;
	insert_orders_segment = 0;
	insert_lineitem_segment = 0;
	delete_segment = 0;
	verbose = 0;
	set_seeds = 0;
	scale = 1;
	updates = 0;

	// check if it is the first invocation
	if (first_invocation) {
		// store the initial random seed
		memcpy(seed_backup, Seed, sizeof(seed_t) * MAX_STREAM + 1);
		first_invocation = false;
	} else {
		// restore random seeds from backup
		memcpy(Seed, seed_backup, sizeof(seed_t) * MAX_STREAM + 1);
	}
	tdefs[PART].base = 200000;
	tdefs[PSUPP].base = 200000;
	tdefs[SUPP].base = 10000;
	tdefs[CUST].base = 150000;
	tdefs[ORDER].base = 150000 * ORDERS_PER_CUST;
	tdefs[LINE].base = 150000 * ORDERS_PER_CUST;
	tdefs[ORDER_LINE].base = 150000 * ORDERS_PER_CUST;
	tdefs[PART_PSUPP].base = 200000;
	tdefs[NATION].base = NATIONS_MAX;
	tdefs[REGION].base = NATIONS_MAX;

	children = 1;
	d_path = NULL;
	
    if (flt_scale < MIN_SCALE) {
		int i;
		int int_scale;

		scale = 1;
		int_scale = (int)(1000 * flt_scale);
		for (i = PART; i < REGION; i++) {
			tdefs[i].base = (DSS_HUGE)(int_scale * tdefs[i].base) / 1000;
			if (tdefs[i].base < 1)
				tdefs[i].base = 1;
		}
	} else {
		scale = (long)flt_scale;
	}

	load_dists();
	
    /* have to do this after init */
	tdefs[NATION].base = nations.count;
	tdefs[REGION].base = regions.count;

	/**
	** region_append_info
	**/
    struct append_info_t region_info = {3, NULL, 0};
    monetdbe_column* region_cols[] = {
        (monetdbe_column*) malloc(sizeof(monetdbe_column_int64_t)),
        (monetdbe_column*) malloc(sizeof(monetdbe_column_str)),
        (monetdbe_column*) malloc(sizeof(monetdbe_column_str))
    };
    region_cols[0]->type = monetdbe_int64_t;
    region_cols[1]->type = monetdbe_str;
    region_cols[2]->type = monetdbe_str;
    region_cols[0]->name = "r_regionkey";
    region_cols[1]->name = "r_name";
    region_cols[2]->name = "r_comment";
    region_info.cols = region_cols;
	/**
	** nation_append_info
	**/
    struct append_info_t nation_info = {4, NULL, 0};
    monetdbe_column* nation_cols[] = {
        (monetdbe_column*) malloc(sizeof(monetdbe_column_int64_t)),
        (monetdbe_column*) malloc(sizeof(monetdbe_column_str)),
        (monetdbe_column*) malloc(sizeof(monetdbe_column_int64_t)),
        (monetdbe_column*) malloc(sizeof(monetdbe_column_str))
    };
    nation_cols[0]->type = monetdbe_int64_t;
    nation_cols[1]->type = monetdbe_str;
    nation_cols[2]->type = monetdbe_int64_t;
    nation_cols[3]->type = monetdbe_str;
    nation_cols[0]->name = "n_nationkey";
    nation_cols[1]->name = "n_name";
    nation_cols[2]->name = "n_regionkey";
    nation_cols[3]->name = "n_comment";
    nation_info.cols = nation_cols;
	/**
	**supplier_append_info
	**/
    struct append_info_t supplier_info = {7, NULL, 0};
    monetdbe_column* supplier_cols[] = {
        (monetdbe_column*) malloc(sizeof(monetdbe_column_int64_t)),
        (monetdbe_column*) malloc(sizeof(monetdbe_column_str)),
        (monetdbe_column*) malloc(sizeof(monetdbe_column_int64_t)),
        (monetdbe_column*) malloc(sizeof(monetdbe_column_str))
    };
	       
    
    
    
//    "s_suppkey INT NOT NULL,"\
//	       "s_name VARCHAR(25) NOT NULL,"\
//	       "s_address VARCHAR(40) NOT NULL,"\
//	       "s_nationkey INT NOT NULL,"\
//	       "s_phone VARCHAR(15) NOT NULL,"\
//	       "s_acctbal DECIMAL(15,2) NOT NULL,"\
//	       "s_comment VARCHAR(101) NOT NULL);"
//#define PART 0
//#define PSUPP 1
//#define SUPP 2
//#define CUST 3
//#define ORDER 4
//#define LINE 5
//#define ORDER_LINE 6
//#define PART_PSUPP 7
//#define NATION 8
//#define REGION 9
    
	/*
	* traverse the tables, invoking the appropriate data generation routine for any to be built
	*/
	for (i = PART; i <= REGION; i++) {
		if (table & (1 << i))
		{
            if (i < NATION)
                rowcnt = tdefs[i].base * scale;
            else
                rowcnt = tdefs[i].base;
            printf("%s, rowcount=%d\n", get_table_name(i), rowcnt);
            printf("---------------\n");
            if (i == REGION) {
                init_info(&region_info, rowcnt); 
                gen_tbl((int)i, rowcnt, &region_info);
                if ((err = monetdbe_append(mdbe, "sys", "region", region_info.cols, region_info.ncols)) != NULL)
                    return err;
            }
		}
    }

    return NULL;
}


/*
* MAIN
*
* assumes the existance of getopt() to clean up the command 
* line handling
*/
//int
//main (int ac, char **av)
//{
//	DSS_HUGE i;
//	
//	table = (1 << CUST) |
//		(1 << SUPP) |
//		(1 << NATION) |
//		(1 << REGION) |
//		(1 << PART_PSUPP) |
//		(1 << ORDER_LINE);
//	force = 0;
//    insert_segments=0;
//    delete_segments=0;
//    insert_orders_segment=0;
//    insert_lineitem_segment=0;
//    delete_segment=0;
//	verbose = 0;
//	set_seeds = 0;
//	scale = 1;
//	flt_scale = 1.0;
//	updates = 0;
//	step = -1;
//	tdefs[ORDER].base *=
//		ORDERS_PER_CUST;			/* have to do this after init */
//	tdefs[LINE].base *=
//		ORDERS_PER_CUST;			/* have to do this after init */
//	tdefs[ORDER_LINE].base *=
//		ORDERS_PER_CUST;			/* have to do this after init */
//	children = 1;
//	d_path = NULL;
//	
//#ifdef NO_SUPPORT
//	signal (SIGINT, exit);
//#endif /* NO_SUPPORT */
//#if (defined(WIN32)&&!defined(_POSIX_))
//	for (i = 0; i < ac; i++)
//	{
//		spawn_args[i] = malloc (((int)strlen (av[i]) + 1) * sizeof (char));
//		MALLOC_CHECK (spawn_args[i]);
//		strcpy (spawn_args[i], av[i]);
//	}
//	spawn_args[ac] = NULL;
//#endif
//	
//	if (verbose >= 0)
//		{
//		fprintf (stderr,
//			"%s Population Generator (Version %d.%d.%d)\n",
//			NAME, VERSION, RELEASE, PATCH);
//		fprintf (stderr, "Copyright %s %s\n", TPC, C_DATES);
//		}
//	
//	load_dists ();
//#ifdef RNG_TEST
//	for (i=0; i <= MAX_STREAM; i++)
//		Seed[i].nCalls = 0;
//#endif
//	/* have to do this after init */
//	tdefs[NATION].base = nations.count;
//	tdefs[REGION].base = regions.count;
//	
//	/* 
//	* updates are never parallelized 
//	*/
//	if (updates)
//		{
//		/* 
//		 * set RNG to start generating rows beyond SF=scale
//		 */
//		set_state (ORDER, scale, 100, 101, &i); 
//		rowcnt = (int)(tdefs[ORDER_LINE].base / 10000 * scale * UPD_PCT);
//		if (step > 0)
//			{
//			/* 
//			 * adjust RNG for any prior update generation
//			 */
//	      for (i=1; i < step; i++)
//         {
//			sd_order(0, rowcnt);
//			sd_line(0, rowcnt);
//         }
//			upd_num = step - 1;
//			}
//		else
//			upd_num = 0;
//
//		while (upd_num < updates)
//			{
//			if (verbose > 0)
//				fprintf (stderr,
//				"Generating update pair #%d for %s",
//				upd_num + 1, tdefs[ORDER_LINE].comment);
//			insert_orders_segment=0;
//			insert_lineitem_segment=0;
//			delete_segment=0;
//			minrow = upd_num * rowcnt + 1;
//			gen_tbl (ORDER_LINE, minrow, rowcnt, upd_num + 1);
//			if (verbose > 0)
//				fprintf (stderr, "done.\n");
//			pr_drange (ORDER_LINE, minrow, rowcnt, upd_num + 1);
//			upd_num++;
//			}
//
//		exit (0);
//		}
//	
//	/**
//	** actual data generation section starts here
//	**/
//
//	/*
//	* traverse the tables, invoking the appropriate data generation routine for any to be built
//	*/
//	for (i = PART; i <= REGION; i++)
//		if (table & (1 << i))
//		{
//			if (children > 1 && i < NATION)
//			{
//				partial ((int)i, step);
//			}
//			else
//			{
//				minrow = 1;
//				if (i < NATION)
//					rowcnt = tdefs[i].base * scale;
//				else
//					rowcnt = tdefs[i].base;
//				if (verbose > 0)
//					fprintf (stderr, "Generating data for %s", tdefs[i].comment);
//				gen_tbl ((int)i, minrow, rowcnt, upd_num);
//				if (verbose > 0)
//					fprintf (stderr, "done.\n");
//			}
//		}
//			
//		return (0);
//}