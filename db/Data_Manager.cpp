/*
 * DB_Manager.cpp
 *
 *  Created on: 2016年10月27日
 *      Author: zhangyalei
 */

#include "Base_Enum.h"
#include "Base_Function.h"
#include "Mongo_Operator.h"
#include "Mysql_Operator.h"
#include "Data_Manager.h"

Data_Manager::Data_Manager(void) :
	db_operator_map_(get_hash_table_size(512)),
	db_buffer_map_(get_hash_table_size(512)),
	runtime_data_map_(get_hash_table_size(4096))
{ }

Data_Manager::~Data_Manager(void) { }

Data_Manager *Data_Manager::instance_;

Data_Manager *Data_Manager::instance(void) {
	if (instance_ == 0)
		instance_ = new Data_Manager;
	return instance_;
}

int Data_Manager::init_db_operator() {
	Mongo_Operator *mongo = new Mongo_Operator();
	Mysql_Operator *mysql = new Mysql_Operator();
	db_operator_map_[MYSQL] = mysql;
	db_operator_map_[MONGO] = mongo;
	return 0;
}

DB_Operator *Data_Manager::db_operator(int type) {
	DB_Operator_Map::iterator iter = db_operator_map_.find(type / 1000);
	if(iter != db_operator_map_.end())
		return iter->second;
	LOG_ERROR("DB_TYPE %d not found!", type);
	return nullptr;
}

int Data_Manager::save_db_data(int db_id, DB_Struct *db_struct, Block_Buffer *buffer, int flag) {
	int64_t index = 0;
	buffer->peek_int64(index);
	Table_Buffer_Map *table_buffer_map = nullptr;
	DB_Buffer_Map::iterator iter = db_buffer_map_.find(db_id);
	if(iter == db_buffer_map_.end()){
		table_buffer_map = new Table_Buffer_Map(get_hash_table_size(512));
		db_buffer_map_[db_id] = table_buffer_map;
	}
	else {
		table_buffer_map = iter->second;
	}

	Record_Buffer_Map *record_buffer_map = nullptr;
	Table_Buffer_Map::iterator ite = table_buffer_map->find(db_struct->table_name());
	if(ite == table_buffer_map->end()){
		record_buffer_map = new Record_Buffer_Map(get_hash_table_size(10000));
		(*table_buffer_map)[db_struct->table_name()] = record_buffer_map;
	}
	else {
		record_buffer_map = ite->second;
	}

	Record_Buffer_Map::iterator it = record_buffer_map->find(index);
	if(it == record_buffer_map->end()){
		switch(flag) {
		case 0:
			(*record_buffer_map)[index] = buffer;
			break;
		case 1:
			(*record_buffer_map)[index] = buffer;
			DB_OPERATOR(db_id)->save_data(db_id, db_struct, buffer);
			break;
		case 2:
			DB_OPERATOR(db_id)->save_data(db_id, db_struct, buffer);
			break;
		}
	}
	else {
		switch(flag) {
		case 0:
			push_buffer(it->second);
			it->second = buffer;
			break;
		case 1:
			push_buffer(it->second);
			it->second = buffer;
			DB_OPERATOR(db_id)->save_data(db_id, db_struct, buffer);
			break;
		case 2:
			push_buffer(it->second);
			record_buffer_map->erase(it);
			DB_OPERATOR(db_id)->save_data(db_id, db_struct, buffer);
			break;
		}
	}
	return 0;
}

int Data_Manager::load_db_data(int db_id, DB_Struct *db_struct, int64_t index, std::vector<Block_Buffer *> &buffer_vec) {
	Table_Buffer_Map *table_buffer_map = nullptr;
	DB_Buffer_Map::iterator iter = db_buffer_map_.find(db_id);
	if(iter == db_buffer_map_.end()){
		table_buffer_map = new Table_Buffer_Map(get_hash_table_size(512));
		db_buffer_map_[db_id] = table_buffer_map;
	}
	else {
		table_buffer_map = iter->second;
	}

	Record_Buffer_Map *record_buffer_map = nullptr;
	Table_Buffer_Map::iterator ite = table_buffer_map->find(db_struct->table_name());
	if(ite == table_buffer_map->end()){
		record_buffer_map = new Record_Buffer_Map(get_hash_table_size(10000));
		(*table_buffer_map)[db_struct->table_name()] = record_buffer_map;
	}
	else {
		record_buffer_map = ite->second;
	}

	int len = 0;
	if(index == 0) {
		if(record_buffer_map->empty()) {
			DB_OPERATOR(db_id)->load_data(db_id, db_struct, index, buffer_vec);
			for(std::vector<Block_Buffer *>::iterator iter = buffer_vec.begin();
					iter != buffer_vec.end(); iter++){
				int64_t ind = 0;
				(*iter)->peek_int64(ind);
				(*	record_buffer_map)[ind] = (*iter);
			}
		}
		else {
			for(Record_Buffer_Map::iterator iter = record_buffer_map->begin();
					iter != record_buffer_map->end(); iter++){
				buffer_vec.push_back(iter->second);
			}
		}
	}
	else {
		Record_Buffer_Map::iterator iter;
		if((iter = record_buffer_map->find(index)) == record_buffer_map->end()){
			DB_OPERATOR(db_id)->load_data(db_id, db_struct, index, buffer_vec);
			for(std::vector<Block_Buffer *>::iterator iter = buffer_vec.begin();
					iter != buffer_vec.end(); iter++){
				int64_t ind = 0;
				(*iter)->peek_int64(ind);
				(*	record_buffer_map)[ind] = (*iter);
			}
		}
		else {
			buffer_vec.push_back(iter->second);
		}
	}
	len = buffer_vec.size();
	return len;
}

int Data_Manager::delete_db_data(int db_id, DB_Struct *db_struct, Block_Buffer *buffer) {
	DB_Buffer_Map::iterator iter;
	Table_Buffer_Map::iterator it;
	if((iter = db_buffer_map_.find(db_id)) == db_buffer_map_.end()) {
		return -1;
	}
	if((it = iter->second->find(db_struct->table_name())) == iter->second->end()) {
		return -1;
	}
	Record_Buffer_Map *record_buffer_map = it->second;
	int rdx = buffer->get_read_idx();
	uint16_t len = 0;
	buffer->read_uint16(len);
	for(uint i = 0; i < len; i++){
		int64_t key = 0;
		buffer->read_int64(key);
		record_buffer_map->erase(key);
	}
	buffer->set_read_idx(rdx);
	DB_OPERATOR(db_id)->delete_data(db_id, db_struct, buffer);
	return 0;
}

void Data_Manager::set_runtime_data(int64_t index, DB_Struct *db_struct, Block_Buffer *buffer) {
	Runtime_Data_Map::iterator iter = runtime_data_map_.find(index);
	if(iter == runtime_data_map_.end()) {
		runtime_data_map_[index] = buffer;
	}
	else {
		push_buffer(iter->second);
		iter->second = buffer;
	}
}

Block_Buffer *Data_Manager::get_runtime_data(int64_t index, DB_Struct *db_struct) {
	Runtime_Data_Map::iterator iter = runtime_data_map_.find(index);
	if(iter != runtime_data_map_.end()) {
		return iter->second;
	}
	return nullptr;
}

void Data_Manager::delete_runtime_data(int64_t index) {
	Runtime_Data_Map::iterator iter = runtime_data_map_.find(index);
	if(iter != runtime_data_map_.end()) {
		push_buffer(iter->second);
		runtime_data_map_.erase(iter);
	}
}