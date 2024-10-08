function on_media_error(obj){
	if (obj.src)
		showerr("Error loading  media");
}

var ws = null;
const global_version = MACRO__GLOBAL_VERSION;
const cmd_regexes = [
	/^PAUSE$/,
	/^(PLAY|PAUSE)(.*)@([0-9]+(?:[.][0-9]+)?)@([0-9]+(?:[.][0-9]+)?)$/,
	/^PLAYRATE ([0-9]+(?:[.][0-9]+)?)$/,
	/^SEEDIRF (.*)$/,
	/^SEE (?:https?:\/\/(?:www[.]youtube[.]com\/watch[?]v=|youtu[.]be\/)|yt )([A-Za-z_-]+)/,
];
var media__last_seeked_to = 0.0;
var playback_rate = 1.0;
const cmd_fns     = [
	function(){
		view_video.pause();
	},
	function(match){
		if (currently_viewing_file_id !== match[2]){
			view_video_core(match[2]);
		}
		$$$media_location_seek(match[1], parseFloat(match[3]));
		const playrate = parseFloat(match[4]);
		if (playrate !== playback_rate){
			$$$media_set_playrate(playrate);
		}
	},
	function(match){
		$$$media_set_playrate(parseFloat(match[1]));
	},
	function(match){
		view_video_core(match[1]);
	},
	function(match){
		showMessage_raw(`[Not implemented: Watching youtube ID ${match[0]}]`);
	},
];
function everyone_view_video(relative_path){
	view_video_core(relative_path);
	ws__send("SEEDIRF "+currently_viewing_file_id, true);
}
function mkvidinfo(){
	return currently_viewing_file_id+"@"+view_video.currentTime+"@"+view_video.playbackRate;
}
function $$$media_location_changed(eventobj){
	ws__send(["PLAY","PAUSE"][view_video.paused|0]+mkvidinfo(), true);
}
function $$$media_paused(eventobj){
	if (pausing_at_request_of_other){
		pausing_at_request_of_other = false;
	} else {
		ws__send("PAUSE"+mkvidinfo(), true);
	}
}
function $$$media_played(eventobj){
	if (playing_at_request_of_other){
		playing_at_request_of_other = false;
	} else {
		ws__send("PLAY"+mkvidinfo(), true);
	}
}
var pausing_at_request_of_other = false;
var playing_at_request_of_other = false;
function $$$media_location_seek(aaa, t){
	if (((t-media__last_seeked_to)*(t-media__last_seeked_to)) > 10.0){
		view_video.currentTime = t;
		media__last_seeked_to = t;
	}
	if (view_video.paused){
		if (aaa === "PLAY"){
			playing_at_request_of_other = true;
			view_video.play();
		}
	} else if (aaa === "PAUSE"){
		pausing_at_request_of_other = true;
		view_video.pause();
	}
}
function $$$media_set_playrate(x){
	if (x != playback_rate){
		view_video.playbackRate = x;
		playback_rate = x;
	}
}
function $$$media_rate_changed(eventobj){
	playback_rate = view_video.playbackRate;
	ws__send("PLAYRATE "+playback_rate, true);
}
var sendMessage;
var view_video;
var view_html;
var inline_document_body;
var view_video_src;
var currently_viewing_file_id = null;
var rp_window_location_hash = "";
function rp_push_current_state(){
	console.log("rp_push_current_state()", rp_window_location_hash);
}
function get_as_inline_document_body(full_path_minus_file_ext){
	const newcss = document.createElement("link");
	newcss.rel = "stylesheet";
	newcss.href = full_path_minus_file_ext+".icss";
	newcss.onload = () => {
			getorpost(full_path_minus_file_ext+".ihtml", null, datastr2 => {
			inline_document_body.innerHTML = datastr2;
			const newjs = document.createElement("script");
			newjs.src = full_path_minus_file_ext+".ijs";
			document.body.append(newjs);
		});
	};
	document.body.append(newcss);
}
var already_loaded_essay1 = false;
function view_video_core(relative_path){
	currently_viewing_file_id = relative_path;
	const full_path = "/static/"+currently_viewing_file_id;
	if (currently_viewing_file_id.endsWith(".html")){
		view_html.src = full_path;
		view_video.classList.add("display-none");
		view_html.classList.remove("display-none");
		inline_document_body.classList.add("display-none");
	} else if (currently_viewing_file_id === "essay1.ihtml"){
		if (!already_loaded_essay1){ // avoid 'redeclaration of ...' errors
			get_as_inline_document_body(full_path.substr(0,full_path.length-6));
		}
		view_video.classList.add("display-none");
		view_html.classList.add("display-none");
		inline_document_body.classList.remove("display-none");
	} else {
		view_video_src.src = full_path;
		view_video.load();
		view_video.classList.remove("display-none");
		view_html.classList.add("display-none");
		inline_document_body.classList.add("display-none");
	}
	view_container.classList.remove("display-none");
	//document.getElementById("view_video").addEventListener("timeupdate", $$$media_location_update); // TODO: Check if other people have desynced significantly
}
function showMessage_raw(msg){
	const container = document.createElement("pre");
	container.innerText = msg;
	messages.appendChild(container);
	messages.scrollTop = messages.scrollHeight - messages.clientHeight;
}
function showMessage_tmp(msg){
	const container = document.createElement("pre");
	container.innerText = msg;
	container.classList.add("chat_tmpmsg");
	messages.appendChild(container);
	messages.scrollTop = messages.scrollHeight - messages.clientHeight;
}
function showMessage(msg){
	for (let node of Array.from(document.getElementsByClassName("chat_tmpmsg"))){
		node.remove();
	}
	
	const username_length = msg.indexOf(": ");
	
	const container = document.createElement("div");
	
	const username = document.createElement("span");
	username.innerText = msg.substr(0, username_length+2);
	username.classList.add("chat_username");
	
	const prefixtxt = document.createElement("span");
	
	container.appendChild(username);
	container.appendChild(prefixtxt);
	
	msg = msg.substr(username_length+2);
	
	if (msg.length < 200){
		prefixtxt.innerText = msg;
	} else {
		const longtext = document.createElement("div");
		longtext.innerText = msg;
		longtext.classList.add("display-none");
		
		prefixtxt.innerText = "Long message";
		
		const preview = document.createElement("span");
		preview.innerText = msg.substr(0,100) + "..." + msg.substr(msg.length-100);
		
		const togglebtn = document.createElement("button");
		togglebtn.innerText = "VIEW/HIDE";
		togglebtn.addEventListener("pointerup", ()=>{
			longtext.classList.toggle("display-none");
			preview.classList.toggle("display-none");
		});
		
		const downloadbtn = document.createElement("button");
		downloadbtn.innerText = "Prepare download";
		downloadbtn.addEventListener("pointerup", ()=>{
			const blob = new Blob([text_encoder.encode(msg)], {type:"application/octet-stream"});
			const bloburl = URL.createObjectURL(blob);
			
			const link = document.createElement("a");
			link.href = bloburl;
			link.download = "download";
			link.innerText = "Download";
			/*link.addEventListener("pointerup", ()=>{
				URL.revokeObjectURL(bloburl); // frees memory
				link.remove();
			}); wont download if this is included */
			container.insertBefore(link, preview);
		});
		
		container.appendChild(togglebtn);
		container.appendChild(downloadbtn);
		container.appendChild(preview);
		container.appendChild(longtext);
	}
	
	messages.appendChild(container);
	messages.scrollTop = messages.scrollHeight - messages.clientHeight;
};
var ws_onOpen_fn = null;
function ws_onOpen(){
	/*getorpost("/chatpassword.txt", null, password => {
		ws.send(password);
		console.log("Attempting to log in with password: "+password);
	});*/
	showMessage_raw("[Connection opened]");
	if (ws_onOpen_fn !== null){
		ws_onOpen_fn();
		ws_onOpen_fn = null;
	}
}
function ws_onClose(){
	showMessage_raw("[Connection closed - probably timed out]");
}
function ws_onMsg(ev){
	let fullmsg = ev.data;
	const mm = fullmsg.match(/^[A-Za-z0-9@._-]+: (.*)$/);
	let m = null;
	if (mm === null){
		fullmsg = "[" + fullmsg + "]";
	} else if (maybe_execute_fn_from_ihtml_websocket_msg(mm[1])){
	} else {
		for (let i = 0;  i < cmd_regexes.length;  ++i){
			m = mm[1].match(cmd_regexes[i]);
			if (m !== null){
				cmd_fns[i](m);
				break;
			}
		}
	}
	if (m === null)
		showMessage(fullmsg);
}
function ws_onErr(ev){
	showerr(`Cannot connect to server - please wait 10 seconds then try again, or contact administrator`);
}
function ws__isConnectedOrConnecting(){
	return (ws!==null) && ((ws.readyState === ws.OPEN) || (ws.readyState === ws.CONNECTING));
}
function ws__isConnected(){
	return (ws!==null) && (ws.readyState === ws.OPEN);
}
function ws_connectToServer(){
	if (!ws__isConnectedOrConnecting()){
		const server_url = "wss://"+document.location.host+"/1/test/new"; // ws:// for insecure
		ws = new WebSocket(server_url);
		ws.onopen    = ws_onOpen;
		ws.onclose   = ws_onClose;
		ws.onmessage = ws_onMsg;
		ws.onerror   = ws_onErr;
	}
}
function hideerr(){
	showerr_container.classList.add("display-none");
}
function showerr(msg){
	showerr_node.innerText = msg;
	showerr_container.classList.remove("display-none");
	throw Error(msg);
}
function ws__send(msg, is_cmd){
	switch(msg){
		case "HELP":
			showMessage_tmp("* HELP: Shows a list of commands\n* SEE https://youtu.be/videoID: Watch a youtube video");
			msg = null;
			break;
		default:
			break;
	}
	if ((msg !== null) && (ws__isConnected())){
		ws.send(msg);
		if (!is_cmd)
			showMessage_tmp("Sending: "+msg);
		hideerr();
	}
}
const text_encoder = new TextEncoder();
function send_onClick(){
	const msg = sendMessage.value;
	if (text_encoder.encode(msg).length > 12000){
		showerr("Your message is too long. Keep it below 12KB.");
	} else if (!ws__isConnectedOrConnecting()){
		ws_onOpen_fn = send_onClick;
		ws_connectToServer();
	} else if (msg === ""){
		showerr("You haven't written a message");
	} else {
		ws__send(msg, false);
		if (ws__isConnected())
			sendMessage.value = "";
	}
}

function getorpost(url, body, fn){
	const d = {credentials:"include", mode:"no-cors", method:["POST","GET"][(body === null)|0]};
	if (body !== null){
		d["body"] = body;
	}
	fetch(url, d).then(r => {
		if (!r.ok){
			showerr(`Server returned ${r.status}: ${r.statusText}`);
		}
		if (fn !== null)
			r.text().then(fn);
	});
}

ws_connectToServer();



document.addEventListener("DOMContentLoaded", ()=>{
document.getElementById("connect").onclick = ws_connectToServer;
document.getElementById("disconnect").onclick = function(){
	ws.close();
};
document.getElementById("send").onclick = send_onClick;
sendMessage = document.getElementById("sendMessage");
sendMessage.onkeyup = function(ev){
	ev.preventDefault();
	if (ev.keyCode === 13)
		send_onClick();
};
document.getElementById("showfilemenu").onclick = function(){
	view_files.classList.toggle("display-none");
};
view_video = document.getElementById("view_video");
view_video.addEventListener("seeked", $$$media_location_changed);
view_video.addEventListener("pause",  $$$media_paused);
view_video.addEventListener("play",   $$$media_played);
view_video.addEventListener("ratechange",   $$$media_rate_changed);
view_video_src = document.getElementById("view_video_src");
view_html = document.getElementById("view_html");
inline_document_body = document.getElementById("inline_document_body");

document.getElementById("hideerrbtn").onclick = hideerr;

view_video.onerror = on_media_error;

showsiblingmenubtn.addEventListener("pointerdown", function(){
	showsiblingmenubtn.nextElementSibling.classList.toggle("display-none");
});

showMessage_raw("[Type HELP to show a list of commands]");

function standardise_thumbnail_url(url){
	if (url.startsWith("data:"))
		url = url + "?v=" + global_version;
	return url;
}

getorpost("MACRO__ALL_FILES_JSON_PATH", null, datastr => {
	const [tags,files,tag2thumbnail,_] = JSON.parse(datastr);
	let s = "";
	for (let [tagid, tagname] of Object.entries(tags)){
		const categorythumb = tag2thumbnail[tagid];
		s += `<div><div class="dir_files_container_name">`;
		if (categorythumb !== undefined){
			s += `<img class="chat_media_tag_thumbnail" src="${standardise_thumbnail_url(categorythumb)}"></img>`;
		} else {
			s += tagname;
		}
		s += `</div><div class="dir_files_container flexcontainer display-none">`;
		for (let i = 0;  i < files.length;  ++i){
			const [fileid,thumb_str,tagids] = files[i];
			if (tagids.includes(tagid|0)){
				const realthumb = (typeof thumb_str === "number") ? tag2thumbnail[thumb_str] : thumb_str;
				s += `<div class="dir_file flexitem" data-filename="${fileid.toString().padStart(4,'0')}?v=${global_version}"><img class="dir_file_thumb" src="${standardise_thumbnail_url(realthumb)}"></img><p class="title_over_sibling_img">TITLE</p></div>`;
			}
		}
		s += "</div></div>";
	}
	view_files.innerHTML = s;
	for (let node of view_files.getElementsByClassName("dir_files_container_name")){
		node.addEventListener("pointerdown", eventobj=>{
			eventobj.currentTarget.nextElementSibling.classList.toggle("display-none");
		});
	}
	for (let node of view_files.getElementsByClassName("dir_file")){
		node.addEventListener("pointerdown", eventobj=>{
			everyone_view_video(eventobj.currentTarget.dataset.filename);
			view_files.classList.add("display-none");
		});
	}
});
});


function IHTML__JS__EVENT__GRAPH_REPLACED_WITH_NODES_OR_WORLDMAP(value){
	ihtml_fn__send(IHTML__JS__EVENT__GRAPH_REPLACED_WITH_NODES_OR_WORLDMAP, [value]);
}
function IHTML__JS__EVENT__GRAPH_TRANSFORMATION_CHANGED(value){
	ihtml_fn__send(IHTML__JS__EVENT__GRAPH_TRANSFORMATION_CHANGED, [value]);
}
function IHTML__JS__EVENT__GRAPH_TEXT_SIZE_CHANGED(value){
	ihtml_fn__send(IHTML__JS__EVENT__GRAPH_TEXT_SIZE_CHANGED, [value]);
}
function IHTML__JS__EVENT__DISPLAY_ALL_ANCESTOR_SECTIONS_OF_X00__THEN_SCROLL_INTO_VIEW(node){
	ihtml_fn__send(IHTML__JS__EVENT__DISPLAY_ALL_ANCESTOR_SECTIONS_OF_X00__THEN_SCROLL_INTO_VIEW, [node.id]);
}
function IHTML__JS__EVENT__R_COL_NODE__ONPOINTERLEAVE(node){
	ihtml_fn__send(IHTML__JS__EVENT__R_COL_NODE__ONPOINTERLEAVE, [node.id]);
}
function IHTML__JS__EVENT__R_COL_NODE__ONPOINTERENTER(node){
	ihtml_fn__send(IHTML__JS__EVENT__R_COL_NODE__ONPOINTERENTER, [node.id]);
}
function IHTML__JS__EVENT__COLWIDTHCHANGED(value){
	ihtml_fn__send(IHTML__JS__EVENT__COLWIDTHCHANGED, [value]);
}
function IHTML__JS__EVENT__TOGGLEDDARKTHEME(is_dark_theme){
	ihtml_fn__send(IHTML__JS__EVENT__TOGGLEDDARKTHEME, [value]);
}
function IHTML__JS__EVENT__FONTSIZECHANGED(value){
	ihtml_fn__send(IHTML__JS__EVENT__FONTSIZECHANGED, [value]);
}
function IHTML__JS__EVENT__L_COL__ONPOINTERUP(node){
	ihtml_fn__send(IHTML__JS__EVENT__L_COL__ONPOINTERUP, [node.id]);
}
function IHTML__JS__EVENT__R_COL_NODE__ONPOINTERUP(node){
	ihtml_fn__send(IHTML__JS__EVENT__R_COL_NODE__ONPOINTERUP, [node.id]);
}
function call_fn_with_node_from_nodeid(rp_fn, nodeid){
	rp_fn({target:document.getElementById(nodeid)}); // window[functionname] is NULL!!!! even though functionname is defined
}
function rp_toggledarktheme(rp_fn, is_dark_theme){
	
}
const IHTML_FN_TO_WEBSOCKET_STRING = [
	[IHTML__JS__EVENT__GRAPH_REPLACED_WITH_NODES_OR_WORLDMAP,"a", null],
	[IHTML__JS__EVENT__GRAPH_TRANSFORMATION_CHANGED,         "b", null],
	[IHTML__JS__EVENT__GRAPH_TEXT_SIZE_CHANGED,              "c", null],
	[IHTML__JS__EVENT__DISPLAY_ALL_ANCESTOR_SECTIONS_OF_X00__THEN_SCROLL_INTO_VIEW,"d", call_fn_with_node_from_nodeid],
	[IHTML__JS__EVENT__R_COL_NODE__ONPOINTERLEAVE,           "e", call_fn_with_node_from_nodeid],
	[IHTML__JS__EVENT__R_COL_NODE__ONPOINTERENTER,           "f", call_fn_with_node_from_nodeid],
	[IHTML__JS__EVENT__COLWIDTHCHANGED,                      "g", null],
	[IHTML__JS__EVENT__TOGGLEDDARKTHEME,                     "h", null],
	[IHTML__JS__EVENT__FONTSIZECHANGED,                      "i", null],
	[IHTML__JS__EVENT__L_COL__ONPOINTERUP,                   "j", call_fn_with_node_from_nodeid],
	[IHTML__JS__EVENT__R_COL_NODE__ONPOINTERUP,              "k", call_fn_with_node_from_nodeid],
];
const IHTML_FN_TO_ACTION_OTHER_PERSON_TOLD_US_TO_DO = {};
function get_websocket_string_from_ihtml_fn(fn1){
	for (let [fn,wsstr,fn2] of IHTML_FN_TO_WEBSOCKET_STRING){
		if (fn === fn1)
			return "$"+wsstr;
	}
}
function maybe_execute_fn_from_ihtml_websocket_msg(msg){
	if (msg[0] !== "$")
		return false;
	for (let [fn,wsstr,fn2] of IHTML_FN_TO_WEBSOCKET_STRING){
		if (msg[1] === wsstr){
			IHTML_FN_TO_ACTION_OTHER_PERSON_TOLD_US_TO_DO[fn.name] = msg.substr(2);
			if (fn2 !== null){
				fn2(IHTML_ihtmlfn2rpillfn[fn.name], ...JSON.parse(msg.substr(2))); // NOTE: I can manually get IHTML_ihtmlfn2rpillfn[fn] in the console, but it is undefined in the scripts! So using function name strings instead
			}
			return true;
		}
	}
	return false;
}
function ihtml_fn__send(fn, values){
	if (!(values instanceof Array)){
		values = [values];
	}
	const sendvalue = JSON.stringify(values);
	if (IHTML_FN_TO_ACTION_OTHER_PERSON_TOLD_US_TO_DO[fn.name] !== sendvalue)
		IHTML_FN_TO_ACTION_OTHER_PERSON_TOLD_US_TO_DO[fn.name] = "";
	else
		ws__send(get_websocket_string_from_ihtml_fn(fn)+sendvalue, false);
}
