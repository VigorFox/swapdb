/*
Copyright (c) 2012-2014 The SSDB Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
*/
#include "link_redis.h"
#include <map>
#include <cstring>
#include <net/redis/reponse_redis.h>
#include <unordered_map>

enum REPLY{
	REPLY_BULK = 0,
	REPLY_MULTI_BULK,
	REPLY_INT,
	REPLY_OK_STATUS,
	REPLY_CUSTOM_STATUS,
	REPLY_SET_STATUS,
	REPLY_SPOP_SRANDMEMBER,
	REPLY_SCAN,
	REPLY_INFO,
};

enum STRATEGY{
	STRATEGY_AUTO,
	STRATEGY_PING,
	STRATEGY_MGET,
	STRATEGY_HMGET,
	STRATEGY_HGETALL,
	STRATEGY_HKEYS,
	STRATEGY_HVALS,
	STRATEGY_SETEX,
	STRATEGY_ZRANGE,
	STRATEGY_ZREVRANGE,
	STRATEGY_ZRANGEBYSCORE,
	STRATEGY_ZREVRANGEBYSCORE,
	STRATEGY_ZINCRBY,
	STRATEGY_REMRANGEBYRANK,
	STRATEGY_REMRANGEBYSCORE,
	STRATEGY_NULL
};

static bool inited = false;

typedef std::unordered_map<std::string, RedisRequestDesc> RedisRequestConvertTable;
static RedisRequestConvertTable cmd_table;

struct RedisCommand_raw
{
	int strategy;
	const char *redis_cmd;
	const char *ssdb_cmd;
	int reply_type;
};

static RedisCommand_raw cmds_raw[] = {
	{STRATEGY_PING, "ping",		 "ping",		REPLY_OK_STATUS},
	{STRATEGY_PING, "checkpoint","ping",		REPLY_OK_STATUS},
	{STRATEGY_PING, "dreply",	"dreply",		REPLY_OK_STATUS},
	{STRATEGY_AUTO, "info",		"info",			REPLY_INFO},

	{STRATEGY_AUTO, "dump",		"dump",			REPLY_BULK},
	{STRATEGY_AUTO, "restore",	"restore",  	REPLY_OK_STATUS},
	{STRATEGY_AUTO, "restore-asking", "restore",  	REPLY_OK_STATUS},


	{STRATEGY_AUTO, "redis_req_dump",		"redis_req_dump",		REPLY_OK_STATUS},
	{STRATEGY_AUTO, "redis_req_restore",	"redis_req_restore",  	REPLY_OK_STATUS},

	{STRATEGY_AUTO, "rr_flushall_check",	"rr_flushall_check",  	REPLY_BULK},
	{STRATEGY_AUTO, "rr_do_flushall",	    "rr_do_flushall",  	    REPLY_BULK},
	{STRATEGY_AUTO, "rr_check_write",	    "rr_check_write",  	    REPLY_BULK},
	{STRATEGY_AUTO, "rr_make_snapshot",	    "rr_make_snapshot",  	REPLY_BULK},
	{STRATEGY_AUTO, "rr_transfer_snapshot",	"rr_transfer_snapshot", REPLY_BULK},
	{STRATEGY_AUTO, "rr_del_snapshot",	    "rr_del_snapshot",  	REPLY_BULK},
	{STRATEGY_AUTO, "rr_info",	"info",			REPLY_INFO},


	{STRATEGY_AUTO, "select",	"select",		REPLY_OK_STATUS},
	{STRATEGY_AUTO, "migrate",	 "migrate",		REPLY_OK_STATUS},
	{STRATEGY_AUTO, "client",	"client",		REPLY_OK_STATUS},
	{STRATEGY_AUTO, "quit",		"quit",			REPLY_OK_STATUS},
	{STRATEGY_AUTO, "debug",	"debug",		REPLY_CUSTOM_STATUS},
	{STRATEGY_AUTO, "flushall",	 "flushdb",		REPLY_OK_STATUS},
	{STRATEGY_AUTO, "flushdb",	 "flushdb",		REPLY_OK_STATUS},
	{STRATEGY_AUTO, "flush",	 "flush",		REPLY_OK_STATUS},

	{STRATEGY_AUTO, "filesize",	"filesize",		REPLY_INT},
	{STRATEGY_AUTO, "dbsize",	"dbsize",		REPLY_INT},

	{STRATEGY_AUTO, "scan",		"scan",			REPLY_SCAN},
	{STRATEGY_AUTO, "type",		"type",			REPLY_CUSTOM_STATUS},
	{STRATEGY_AUTO, "get",		"get",			REPLY_BULK},
	{STRATEGY_AUTO, "getset",	"getset",		REPLY_BULK},
	{STRATEGY_AUTO, "set",		"set",			REPLY_SET_STATUS},
	{STRATEGY_AUTO, "setnx",	"setnx",		REPLY_INT},
	{STRATEGY_MGET, "mget",		"multi_get",	REPLY_MULTI_BULK},
	{STRATEGY_SETEX,"setex",	"setx", 		REPLY_OK_STATUS},
	{STRATEGY_SETEX,"psetex",	"psetx", 		REPLY_OK_STATUS},
	{STRATEGY_AUTO, "exists",	"exists",		REPLY_INT},
	{STRATEGY_AUTO, "incr",		"incr",			REPLY_INT},
	{STRATEGY_AUTO, "decr",		"decr",			REPLY_INT},
	{STRATEGY_AUTO, "ttl",		"ttl",			REPLY_INT},
	{STRATEGY_AUTO, "pttl",		"pttl",			REPLY_INT},
	{STRATEGY_AUTO, "expire",	"expire",		REPLY_INT},
	{STRATEGY_AUTO, "pexpire",	"pexpire",		REPLY_INT},
	{STRATEGY_AUTO, "expireat",	"expireat",		REPLY_INT},
	{STRATEGY_AUTO, "pexpireat","pexpireat",    REPLY_INT},
	{STRATEGY_AUTO, "persist","persist",    REPLY_INT},
	{STRATEGY_AUTO, "append",	"append",     REPLY_INT},
	{STRATEGY_AUTO, "getbit",	"getbit",		REPLY_INT},
	{STRATEGY_AUTO, "setbit",	"setbit",		REPLY_INT},
	{STRATEGY_AUTO, "strlen",	"strlen",		REPLY_INT},
	{STRATEGY_AUTO, "bitcount",	"bitcount",		REPLY_INT},
	{STRATEGY_AUTO, "substr",	"getrange",		REPLY_BULK},
	{STRATEGY_AUTO, "getrange",	"getrange",		REPLY_BULK},
	{STRATEGY_AUTO, "setrange",	"setrange",		REPLY_INT},
	{STRATEGY_AUTO, "keys", 	"keys", 		REPLY_MULTI_BULK},
	{STRATEGY_AUTO, "del",		"multi_del",	REPLY_INT},
	{STRATEGY_AUTO, "mset",		"multi_set",	REPLY_OK_STATUS},
	{STRATEGY_AUTO, "incrby",	"incr",			REPLY_INT},
	{STRATEGY_AUTO, "incrbyfloat","incrbyfloat",REPLY_BULK},
	{STRATEGY_AUTO, "decrby",	"decr",			REPLY_INT},


	{STRATEGY_AUTO, "hdel",		"hdel",			REPLY_INT},
	{STRATEGY_AUTO, "hexists",	"hexists",		REPLY_INT},
	{STRATEGY_AUTO, "hget",		"hget",			REPLY_BULK},
	{STRATEGY_HGETALL,"hgetall","hgetall",		REPLY_MULTI_BULK},
	{STRATEGY_AUTO,  "hincrby",	"hincr",		REPLY_INT},
	{STRATEGY_AUTO,  "hincrbyfloat","hincrbyfloat",	REPLY_BULK},
	{STRATEGY_HKEYS, "hkeys", 	"hkeys", 		REPLY_MULTI_BULK},
	{STRATEGY_AUTO,  "hlen",	"hsize",		REPLY_INT},
	{STRATEGY_HMGET, "hmget",	"hmget",	REPLY_MULTI_BULK},
	{STRATEGY_AUTO,  "hmset",	"hmset",	REPLY_OK_STATUS},
	{STRATEGY_AUTO,  "hset",	"hset",			REPLY_INT},
	{STRATEGY_AUTO,  "hsetnx",	"hsetnx",		REPLY_INT},
	//TODO HSTRLEN since 3.2
	{STRATEGY_HVALS, "hvals", 		"hvals", 		REPLY_MULTI_BULK},
	{STRATEGY_AUTO, "hscan",		"hscan",			REPLY_SCAN},

	{STRATEGY_AUTO, "sadd",		    "sadd",			REPLY_INT},
	{STRATEGY_AUTO, "srem",		    "srem",			REPLY_INT},
	{STRATEGY_AUTO, "scard",	    "scard",		REPLY_INT},
//    {STRATEGY_AUTO, "sdiff",	    "sdiff",		REPLY_MULTI_BULK},
//    {STRATEGY_AUTO, "sdiffstore",	"sdiffstore",	REPLY_INT},
//    {STRATEGY_AUTO, "sinter",	    "sinter",		REPLY_MULTI_BULK},
//    {STRATEGY_AUTO, "sinterstore",	"sinterstore",	REPLY_INT},
    {STRATEGY_AUTO, "sismember",	"sismember",	REPLY_INT},
    {STRATEGY_AUTO, "smembers",	    "smembers",		REPLY_MULTI_BULK},
//    {STRATEGY_AUTO, "smove",	    "smove",		REPLY_INT},
    {STRATEGY_AUTO, "spop",	        "spop",		    REPLY_SPOP_SRANDMEMBER},
    {STRATEGY_AUTO, "srandmember",	"srandmember",	REPLY_SPOP_SRANDMEMBER},
//    {STRATEGY_AUTO, "sunion",	    "sunion",		REPLY_MULTI_BULK},
//    {STRATEGY_AUTO, "sunionstore",	"sunionstore",	REPLY_INT},
    {STRATEGY_AUTO, "sscan",	    "sscan",		REPLY_SCAN},

	{STRATEGY_AUTO, "zcard",	"zsize",		REPLY_INT},
	{STRATEGY_AUTO, "zscore",	"zget",			REPLY_BULK},
	{STRATEGY_AUTO, "zrem",		"multi_zdel",	REPLY_INT},
	{STRATEGY_AUTO, "zrank",	"zrank",		REPLY_INT},
	{STRATEGY_AUTO, "zrevrank",	"zrrank",		REPLY_INT},
	{STRATEGY_AUTO, "zcount",	"zcount",		REPLY_INT},
	{STRATEGY_REMRANGEBYRANK, "zremrangebyrank",	"zremrangebyrank",		REPLY_INT},
	{STRATEGY_REMRANGEBYSCORE, "zremrangebyscore",	"zremrangebyscore",		REPLY_INT},
	{STRATEGY_ZRANGE,	"zrange",		"zrange",		REPLY_MULTI_BULK},
	{STRATEGY_ZREVRANGE,"zrevrange",	"zrrange",		REPLY_MULTI_BULK},
	{STRATEGY_AUTO,		"zadd",			"multi_zset", 	REPLY_INT},
	{STRATEGY_ZINCRBY,	"zincrby",		"zincr", 		REPLY_BULK},
//	{STRATEGY_ZRANGEBYSCORE,	"zrangebyscore",	"zscan",	REPLY_MULTI_BULK},
//	{STRATEGY_ZREVRANGEBYSCORE,	"zrevrangebyscore",	"zrscan",	REPLY_MULTI_BULK},
	{STRATEGY_AUTO,	"zrangebyscore",	"zrangebyscore",	REPLY_MULTI_BULK},
	{STRATEGY_AUTO,	"zrevrangebyscore",	"zrevrangebyscore",	REPLY_MULTI_BULK},
	{STRATEGY_AUTO, "zscan",		"zscan",			REPLY_SCAN},
	{STRATEGY_AUTO, "zlexcount",	"zlexcount",		REPLY_INT},
    {STRATEGY_AUTO,	"zrangebylex",	"zrangebylex",	REPLY_MULTI_BULK},
    {STRATEGY_AUTO,	"zremrangebylex",	"zremrangebylex",	REPLY_INT},
	{STRATEGY_AUTO,	"zrevrangebylex",	"zrevrangebylex",	REPLY_MULTI_BULK},

	{STRATEGY_AUTO,		"lpush",		"qpush_front", 		REPLY_INT},
	{STRATEGY_AUTO,		"rpush",		"qpush_back", 		REPLY_INT},
	{STRATEGY_AUTO,		"lpushx",		"qpush_frontx", 	REPLY_INT},
	{STRATEGY_AUTO,		"rpushx",		"qpush_backx", 		REPLY_INT},


	{STRATEGY_AUTO,		"lpop",			"qpop_front", 		REPLY_BULK},
	{STRATEGY_AUTO,		"rpop",			"qpop_back", 		REPLY_BULK},
	{STRATEGY_AUTO, 	"llen",			"qsize",			REPLY_INT},
	{STRATEGY_AUTO,		"lindex",		"qget", 			REPLY_BULK},
	{STRATEGY_AUTO,		"lset",		    "qset", 			REPLY_OK_STATUS},
	{STRATEGY_AUTO,		"lrange",		"qslice",			REPLY_MULTI_BULK},
	{STRATEGY_AUTO, 	"ltrim",		"qtrim",			REPLY_OK_STATUS},


//	{STRATEGY_AUTO, 	"linsert",		"qsize",			REPLY_INT},//TODO
//	{STRATEGY_AUTO, 	"lrem",			"qsize",			REPLY_INT},//TODO

	{STRATEGY_AUTO, 	NULL,			NULL,			0}


};

int RedisLink::convert_req(){
	if(!inited){
		inited = true;
		
		RedisCommand_raw *def = &cmds_raw[0];
		while(def->redis_cmd != NULL){
			RedisRequestDesc desc;
			desc.strategy = def->strategy;
			desc.redis_cmd = def->redis_cmd;
			desc.ssdb_cmd = def->ssdb_cmd;
			desc.reply_type = def->reply_type;
			cmd_table[desc.redis_cmd] = desc;
			def += 1;
		}
	}
	
	this->req_desc = NULL;

	const RedisRequestConvertTable::iterator &it = cmd_table.find(cmd);
	if(it == cmd_table.end()){
		recv_string.reserve(recv_bytes.size());
		recv_string.push_back(cmd);
		for(int i=1; i<recv_bytes.size(); i++){
			recv_string.emplace_back(recv_bytes[i].String());
		}
		return 0;
	}
	this->req_desc = &(it->second);

	if(this->req_desc->strategy == STRATEGY_HKEYS
			||  this->req_desc->strategy == STRATEGY_HVALS
	){
		recv_string.push_back(req_desc->ssdb_cmd);
		if(recv_bytes.size() == 2){
			recv_string.push_back(recv_bytes[1].String());
			recv_string.push_back("");
			recv_string.push_back("");
			recv_string.push_back("2000000000");
		}
		return 0;
	}
	if(this->req_desc->strategy == STRATEGY_SETEX){
		recv_string.push_back(req_desc->ssdb_cmd);
		if(recv_bytes.size() == 4){
			recv_string.push_back(recv_bytes[1].String());
			recv_string.push_back(recv_bytes[3].String());
			recv_string.push_back(recv_bytes[2].String());
		}
		return 0;
	}
	if(this->req_desc->strategy == STRATEGY_ZINCRBY){
		recv_string.push_back(req_desc->ssdb_cmd);
		if(recv_bytes.size() == 4){
			recv_string.push_back(recv_bytes[1].String());
			recv_string.push_back(recv_bytes[3].String());
			recv_string.push_back(recv_bytes[2].String());
		}
		return 0;
	}
	if(this->req_desc->strategy == STRATEGY_REMRANGEBYRANK
		|| this->req_desc->strategy == STRATEGY_REMRANGEBYSCORE)
	{
		recv_string.push_back(req_desc->ssdb_cmd);
		if(recv_bytes.size() >= 4){
			recv_string.push_back(recv_bytes[1].String());
			recv_string.push_back(recv_bytes[2].String());
			recv_string.push_back(recv_bytes[3].String());
		}
		return 0;
	}
	if(this->req_desc->strategy == STRATEGY_ZRANGE
		|| this->req_desc->strategy == STRATEGY_ZREVRANGE)
	{
		recv_string.push_back(req_desc->ssdb_cmd);
		if(recv_bytes.size() >= 4){
//			int64_t start = recv_bytes[2].Int64();
//			int64_t end = recv_bytes[3].Int64();

			recv_string.push_back(recv_bytes[1].String());
			recv_string.push_back(recv_bytes[2].String());
			recv_string.push_back(recv_bytes[3].String());

//			if((start >= 0 && end >= 0) || end == -1){
//				int64_t size;
//				if(end == -1){
//					size = -1;
//				}else{
//					if(this->req_desc->strategy == STRATEGY_REMRANGEBYSCORE){
//						size = end;
//					}else{
//						size = end - start + 1;
//					}
//				}
//				recv_string.push_back(recv_bytes[1].String());
//				recv_string.push_back(recv_bytes[2].String());
//				recv_string.push_back(str(size));
//			}
		}
		if(recv_bytes.size() > 4){
			std::string s = recv_bytes[4].String();
			strtolower(&s);
			recv_string.push_back(s);
		}
		return 0;
	}
	if(this->req_desc->strategy == STRATEGY_ZRANGEBYSCORE || this->req_desc->strategy == STRATEGY_ZREVRANGEBYSCORE){
		recv_string.push_back(req_desc->ssdb_cmd);
		std::string name, smin, smax, withscores, offset, count;
		if(recv_bytes.size() >= 4){
			name = recv_bytes[1].String();
			smin = recv_bytes[2].String();
			smax = recv_bytes[3].String();
			
			bool after_limit = false;
			for(int i=4; i<recv_bytes.size(); i++){
				std::string s = recv_bytes[i].String();
				if(after_limit){
					if(offset.empty()){
						offset = s;
					}else{
						count = s;
						after_limit = false;
					}
				}
				strtolower(&s);
				if(s == "withscores"){
					withscores = s;
				}else if(s == "limit"){
					after_limit = true;
				}
			}
		}
		if(smin.empty() || smax.empty()){
			return 0;
		}
		
		recv_string.push_back(name);
		recv_string.push_back("");
		
		if(smin == "-inf" || smin == "+inf"){
			recv_string.push_back("");
		}else{
			if(smin[0] == '('){
				std::string tmp(smin.data() + 1, smin.size() - 1);
				char buf[32];
				if(this->req_desc->strategy == STRATEGY_ZRANGEBYSCORE){
					snprintf(buf, sizeof(buf), "%d", str_to_int(tmp) + 1);
				}else{
					snprintf(buf, sizeof(buf), "%d", str_to_int(tmp) - 1);
				}
				smin = buf;
			}
			recv_string.push_back(smin);
		}
		if(smax == "-inf" || smax == "+inf"){
			recv_string.push_back("");
		}else{
			if(smax[0] == '('){
				std::string tmp(smax.data() + 1, smax.size() - 1);
				char buf[32];
				if(this->req_desc->strategy == STRATEGY_ZRANGEBYSCORE){
					snprintf(buf, sizeof(buf), "%d", str_to_int(tmp) - 1);
				}else{
					snprintf(buf, sizeof(buf), "%d", str_to_int(tmp) + 1);
				}
				smax = buf;
			}
			recv_string.push_back(smax);
		}
		if(offset.empty()){
			recv_string.push_back("0");
		}else{
			recv_string.push_back(offset);
		}
		if(count.empty()){
			recv_string.push_back("2000000000");
		}else{
			recv_string.push_back(count);
		}

		recv_string.push_back(withscores);
		return 0;
	}

	recv_string.reserve(recv_bytes.size());
	recv_string.push_back(req_desc->ssdb_cmd);
	for(int i=1; i<recv_bytes.size(); i++){
		recv_string.emplace_back(recv_bytes[i].String());
	}
	
	return 0;
}


int RedisLink::recv_res(Buffer *input, RedisResponse *r, int shit) {
	int parsed = 0;

	if (input->empty()) {
		r->status = REDIS_RESPONSE_RETRY;
		return parsed;
	}

	//head for first line
	//head2 for second line

	int size = input->size() - shit;
	char *head = input->data() + shit;

	char *head2 = (char *) memchr(head, '\n', size);
	if (head2 == NULL) {
		r->status = REDIS_RESPONSE_RETRY;
		return parsed;
	}
	head2++;

	unsigned long head_len = head2 - head;
	if (head_len < 2) {
		r->status = REDIS_RESPONSE_RETRY;
		return parsed;
	}

	if (head_len == 2) {
		r->status = REDIS_RESPONSE_ERR;
		return parsed;
	}

	if (*(head2 - 2) != '\r') {
		r->status = REDIS_RESPONSE_ERR;
		return parsed;
	}

	std::string line = std::string(head + 1, head_len - 3);

	parsed = head_len;

	switch (*head) {
		case '-' : {
			r->type = REDIS_REPLY_ERROR;
			r->str = line;
			r->status = REDIS_RESPONSE_DONE;
			break;
		};
		case '+' : {
			r->type = REDIS_REPLY_STATUS;
			r->str = line;
			r->status = REDIS_RESPONSE_DONE;
			break;
		};
		case ':' : {
			r->type = REDIS_REPLY_INTEGER;
			r->integer = str_to_int(line);
			r->status = REDIS_RESPONSE_DONE;
			break;
		};
		case '$' : {
			r->type = REDIS_REPLY_STRING;
			if (line.length() == 2 && line[0] == '-' && line[1] == '1') {
				r->type = REDIS_REPLY_NIL;
				r->status = REDIS_RESPONSE_DONE;
				break;
			}

			int replyLen = str_to_int(line);
			if ((replyLen + 2) > (size - head_len)) {
				//not enough. retry
				r->status = REDIS_RESPONSE_RETRY;
				break;
			}

			std::string str = std::string(head2, replyLen);
			parsed = parsed + replyLen + 2;
			r->str = str;
			r->status = REDIS_RESPONSE_DONE;
			break;
		};
		case '*': {
			r->type = REDIS_REPLY_ARRAY;

			if (line.length() == 2 && line[0] == '-' && line[1] == '1') {
				r->type = REDIS_REPLY_NIL;
				r->status = REDIS_RESPONSE_DONE;
				break;
			}

			int repliesNum = str_to_int(line);
			for (int i = 0; i < repliesNum; ++i) {
				RedisResponse *tmp = new RedisResponse();

				int pt = recv_res(input, tmp, parsed);
				if (tmp->status == REDIS_RESPONSE_DONE) {
					parsed = parsed + pt;
					r->push_back(tmp);
					r->status = tmp->status;
					continue;
				} else if (tmp->status == REDIS_RESPONSE_ERR){
					r->status = tmp->status;
                    delete tmp;
					break;
				} else if (tmp->status == REDIS_RESPONSE_RETRY){
					r->status = tmp->status;
                    delete tmp;
                    break;
				} else {
					r->status = REDIS_RESPONSE_ERR;
                    delete tmp;
                    break;
				}
			}

			break;
		}
		default: {
			r->status = REDIS_RESPONSE_ERR;
			return parsed;
			break;
		}
	}

	if (input->space() == 0) {
		input->nice();
		if (input->space() == 0) {
			if (input->grow() == -1) {
				//log_error("fd: %d, unable to resize input buffer!", this->sock);
				return REDIS_RESPONSE_ERR;
			}
			//log_debug("fd: %d, resize input buffer, %s", this->sock, input->stats().c_str());
		}
	}

	return parsed;
}

const std::vector<Bytes>* RedisLink::recv_req(Buffer *input){
	int ret = this->parse_req(input);
	if(ret == -1){
		return NULL;
	}
	if(recv_bytes.empty()){
		if(input->space() == 0){
			input->nice();
			if(input->space() == 0){
				if(input->grow() == -1){
					//log_error("fd: %d, unable to resize input buffer!", this->sock);
					return NULL;
				}
				//log_debug("fd: %d, resize input buffer, %s", this->sock, input->stats().c_str());
			}
		}
		return &recv_bytes;
	}

	cmd = recv_bytes[0].String();
	strtolower(&cmd);
	
	recv_string.clear();
	
	this->convert_req();

	// Bytes don't hold memory, so we firstly copy Bytes into string and store
	// in a vector of string, then create Bytes-es prointing to strings
	recv_bytes.clear();
	for(int i=0; i<recv_string.size(); i++){
		std::string *str = &recv_string[i];
		recv_bytes.push_back(Bytes(str->data(), str->size()));
	}
	
	return &recv_bytes;
}

int RedisLink::send_append_resp(Buffer *output, const std::vector<std::string> &resp_append){
	if(resp_append.empty()){
		return 0;
	}

	char buf[32];
	snprintf(buf, sizeof(buf), "*%d\r\n", (int) resp_append.size());
 	output->append(buf);

	for(int i=0; i<resp_append.size(); i++){
		const std::string &val = resp_append[i];
		char buf[32];
		snprintf(buf, sizeof(buf), "$%d\r\n", (int)val.size());
		output->append(buf);
		output->append(val.data(), val.size());
		output->append("\r\n");
	}

	return 0;
}

int RedisLink::send_resp(Buffer *output, const std::vector<std::string> &resp){
	if(resp.empty()){
		return 0;
	}
	if(resp[0] != "ok"){
		if(resp[0] == "error" || resp[0] == "fail" || resp[0] == "client_error"){
			if(resp.size() >= 2){
				output->append("-");
				output->append(resp[1]);
			} else {
				output->append("-ERR ");
			}
			output->append("\r\n");
		}else if(resp[0] == "not_found"){
			output->append("$-1\r\n");
		}else if(resp[0] == "noauth"){
			output->append("-NOAUTH ");
			if(resp.size() >= 2){
				output->append(resp[1]);
			}
			output->append("\r\n");
		}else{
			output->append("-ERR server error\r\n");
		}
		return 0;
	}
	
	// not supported command
	if(req_desc == NULL){
		{
			char buf[32];
			snprintf(buf, sizeof(buf), "*%d\r\n", (int)resp.size() - 1);
			output->append(buf);
		}
		for(int i=1; i<resp.size(); i++){
			const std::string &val = resp[i];
			char buf[32];
			snprintf(buf, sizeof(buf), "$%d\r\n", (int)val.size());
			output->append(buf);
			output->append(val.data(), val.size());
			output->append("\r\n");
		}
		return 0;
	}
	if(req_desc->reply_type == REPLY_INFO){

		std::string tmp;
		for(int i=1; i<resp.size(); i++){
			const std::string &val = resp[i];
			tmp.append(val);
			tmp.append("\r\n");
		}

		char buf[32];
		snprintf(buf, sizeof(buf), "$%d\r\n", (int)tmp.size());
		output->append(buf);
		output->append(tmp.data(), tmp.size());
		output->append("\r\n");

		return 0;
	}
	if(req_desc->strategy == STRATEGY_PING){
		output->append("+PONG\r\n");
		return 0;
	}
	if(req_desc->reply_type == REPLY_OK_STATUS){
		output->append("+OK\r\n");
		return 0;
	}
	if(req_desc->reply_type == REPLY_CUSTOM_STATUS){
		if(resp.size() >= 2) {
			output->append("+");
			output->append(resp[1]);
			output->append("\r\n");
		} else {
			output->append("+OK\r\n");
		}

		return 0;
	}
	if(req_desc->reply_type == REPLY_SET_STATUS){
		if(resp.size() >= 2) {
			if (resp[1] == "1") {
				output->append("+OK\r\n");
			} else if (resp[1] == "0") {
				output->append("$-1\r\n");
			} else {
				output->append("-ERR server error, check REPLY_SET_STATUS\r\n");
			}
		} else {
			output->append("+OK\r\n");
		}

		return 0;
	}
	if(req_desc->reply_type == REPLY_BULK){
		if(resp.size() >= 2){
			const std::string &val = resp[1];
			char buf[32];
			snprintf(buf, sizeof(buf), "$%d\r\n", (int)val.size());
			output->append(buf);
			output->append(val.data(), val.size());
			output->append("\r\n");
		}else{
			output->append("$0\r\n");
		}
		return 0;
	}
	if(req_desc->reply_type == REPLY_INT){
		if(resp.size() >= 2){
			const std::string &val = resp[1];
			output->append(":");
			output->append(val.data(), val.size());
			output->append("\r\n");
		}else{
			output->append("$0\r\n");
		}
		return 0;
	}
	if(req_desc->strategy == STRATEGY_MGET || req_desc->strategy == STRATEGY_HMGET){
		if(resp.size() % 2 != 1){
			output->append("*0");
			//log_error("bad response for multi_(h)get");
			return 0;
		}
		char buf[32];
		std::vector<std::string>::const_iterator req_it, resp_it;
		if(req_desc->strategy == STRATEGY_MGET){
			req_it = recv_string.begin() + 1;
			snprintf(buf, sizeof(buf), "*%d\r\n", (int)recv_string.size() - 1);
		}else{
			req_it = recv_string.begin() + 2;
			snprintf(buf, sizeof(buf), "*%d\r\n", (int)recv_string.size() - 2);
		}
		output->append(buf);
		
		resp_it = resp.begin() + 1;

		while(req_it != recv_string.end()){
			const std::string &req_key = *req_it;
			req_it ++;
			if(resp_it == resp.end()){
				output->append("$-1\r\n");
				continue;
			}
			const std::string &resp_key = *resp_it;
			//log_debug("%s %s", req_key.c_str(), resp_key.c_str());
			if(req_key != resp_key){
				output->append("$-1\r\n");
				// loop until we find value to the requested key
				continue;
			}
				
			const std::string &val = *(resp_it + 1);
			char buf[32];
			snprintf(buf, sizeof(buf), "$%d\r\n", (int)val.size());
			output->append(buf);
			output->append(val.data(), val.size());
			output->append("\r\n");
				
			resp_it += 2;
		}

		return 0;
	}

	if(req_desc->reply_type == REPLY_SPOP_SRANDMEMBER){

		 if (recv_string.size() == 2){

			 if (resp.size() == 1) {
				 output->append("$-1\r\n");
			 } else {
				 const std::string &val = resp[1];
				 char buf[32];
				 snprintf(buf, sizeof(buf), "$%d\r\n", (int)val.size());
				 output->append(buf);
				 output->append(val.data(), val.size());
				 output->append("\r\n");
			 }
		} else {
			{
				char buf[32];
				snprintf(buf, sizeof(buf), "*%d\r\n", ((int)resp.size() - 1));
				output->append(buf);
			}
			for(int i=1; i<resp.size(); i++){
				const std::string &val = resp[i];
				char buf[32];
				snprintf(buf, sizeof(buf), "$%d\r\n", (int)val.size());
				output->append(buf);
				output->append(val.data(), val.size());
				output->append("\r\n");
			}
		}

		return 0;
	}

	if(req_desc->reply_type == REPLY_SCAN){

		do {
			if (resp.size() < 2) {
				output->append("-ERR server error when scan\r\n");
				break;
			}

			output->append("*2\r\n");

			const std::string &cursor = resp[1];
			char buf[32];
			snprintf(buf, sizeof(buf), "$%d\r\n", (int)cursor.size());
			output->append(buf);
			output->append(cursor.data(), cursor.size());
			output->append("\r\n");

			size_t count = resp.size() - 2;

			snprintf(buf, sizeof(buf), "*%d\r\n", (int)count);
			output->append(buf);

			for(int i=2; i<resp.size(); i++){
				const std::string &val = resp[i];
				char buf[32];
				snprintf(buf, sizeof(buf), "$%d\r\n", (int)val.size());
				output->append(buf);
				output->append(val.data(), val.size());
				output->append("\r\n");
			}

		} while (0);

		return 0;
	}

	if(req_desc->reply_type == REPLY_MULTI_BULK){
		bool withscores = true;
		if(req_desc->strategy == STRATEGY_ZRANGE || req_desc->strategy == STRATEGY_ZREVRANGE){
			if(recv_string.size() < 5 || recv_string[4] != "withscores"){
				withscores = false;
			}
		}
		if(req_desc->strategy == STRATEGY_ZRANGEBYSCORE || req_desc->strategy == STRATEGY_ZREVRANGEBYSCORE){
			if(recv_string[recv_string.size() - 1] != "withscores"){
				withscores = false;
			}
		}
		{
			char buf[32];
			if(withscores){
				snprintf(buf, sizeof(buf), "*%d\r\n", (int)resp.size() - 1);
			}else{
				snprintf(buf, sizeof(buf), "*%d\r\n", ((int)resp.size() - 1)/2);
			}
			output->append(buf);
		}
		for(int i=1; i<resp.size(); i++){
			const std::string &val = resp[i];
			char buf[32];
			snprintf(buf, sizeof(buf), "$%d\r\n", (int)val.size());
			output->append(buf);
			output->append(val.data(), val.size());
			output->append("\r\n");
			if(!withscores){
				i += 1;
			}
		}
		return 0;
	}
	
	output->append("-ERR server error\r\n");
	return 0;
}

int RedisLink::parse_req(Buffer *input){
	recv_bytes.clear();

	int parsed = 0;
	int size = input->size();
	char *ptr = input->data();
	
	// ignore leading empty lines
	while(size > 0 && (ptr[0] == '\n' || ptr[0] == '\r')){
		ptr ++;
		size --;
		parsed ++;
	}
	//dump(ptr, size);
	
	if(ptr[0] != '*'){
		return -1;
	}

	int num_args = 0;	
	while(size > 0){
		char *lf = (char *)memchr(ptr, '\n', size);
		if(lf == NULL){
			break;
		}
		lf += 1;
		size -= (lf - ptr);
		parsed += (lf - ptr);
		
		int len = (int)strtol(ptr + 1, NULL, 10); // ptr + 1: skip '$' or '*'
		if(errno == EINVAL){
			return -1;
		}
		ptr = lf;
		if(len < 0){
			return -1;
		}
		if(num_args == 0){
			if(len <= 0){
				return -1;
			}
			num_args = len;
			continue;
		}
		
		if(len > size - 1){
			break;
		}
		
		recv_bytes.push_back(Bytes(ptr, len));
		
		ptr += len + 1;
		size -= len + 1;
		parsed += len + 1;
		// compatiabl with both CRLF and LF
		if(size > 0 && *ptr == '\n'){
			ptr += 1;
			size -= 1;
			parsed += 1;
		}

		num_args --;
		if(num_args == 0){
			input->decr(parsed);
			return 1;
		}
	}
	
	recv_bytes.clear();
	return 0;
}
