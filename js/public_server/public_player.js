/*
*	描述：public_player信息类
*	作者：张亚磊
*	时间：2016/03/26
*/

function Public_Player() {
	this.game_cid = 0;          //玩家所在的game服务器cid
	this.sid = 0;               //玩家sid
	this.role_info = null;		//玩家信息
}

//玩家上线，加载数据
Public_Player.prototype.login = function (game_cid, sid, role_info) {
    log_info("public_player login, sid:", sid, " role_id:", role_info.role_id, " role_name:", role_info.role_name, " game_cid:", game_cid);
	this.game_cid = game_cid;
	this.sid = sid;
	this.role_info = role_info;
	global.sid_public_player_map.set(this.sid, this);
	global.role_id_public_player_map.set(this.role_info.role_id, this);
	global.role_name_public_player_map.set(this.role_info.role_name, this);
	
	global.rank_manager.update_rank(Rank_Type.LEVEL_RANK, this);
}

//玩家离线，保存数据
Public_Player.prototype.logout = function () {
    log_info("public_player logout, sid:", this.sid, " role_id:", this.role_info.role_id, " role_name:", this.role_info.role_name, " game_cid:", this.game_cid);
	global.sid_public_player_map.delete(this.sid);
	global.role_id_public_player_map.delete(this.role_info.role_id);
	global.role_name_public_player_map.delete(this.role_info.role_name);
}

Public_Player.prototype.send_msg = function(msg_id, msg) {
    send_msg(Endpoint.PUBLIC_SERVER, this.game_cid, msg_id, Msg_Type.NODE_S2C, this.sid, msg);
}

Public_Player.prototype.send_error_msg = function(error_code) {
	var msg = new Object();
	msg.error_code = error_code;
	send_msg(Endpoint.PUBLIC_SERVER, this.game_cid, Msg.RES_ERROR_CODE, Msg_Type.NODE_S2C, this.sid, msg);
}
