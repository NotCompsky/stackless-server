function getorpost(url, body, fn){
	const d = {credentials:"include", method:["POST","GET"][(body === null)|0]};
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
const filterfn = [
	function(fileid){ return true; },
	function(fileid){ return all_files__as_dict[fileid][3].startsWith("image/"); },
	function(fileid){ return all_files__as_dict[fileid][3] === "image/gif"; },
	function(fileid){ console.log(fileid, all_files__as_dict[fileid]); return all_files__as_dict[fileid][3].startsWith("video/"); },
	function(fileid){ return all_files__as_dict[fileid][3].startsWith("audio/"); },
	function(fileid){ return all_files__as_dict[fileid][3].startsWith("video/") || all_files__as_dict[fileid][3].startsWith("audio/"); },
	function(fileid){ return all_files__as_dict[fileid][3].startsWith("video/") || all_files__as_dict[fileid][3]==="image/gif"; }
];


const global_version = MACRO__GLOBAL_VERSION;
var all_tags_id2name = null;
var all_files = null;
var all_files__as_dict = {};
var all_tags_id2files = [];
var tag2thumbnail = null;
var introduction_to_each_category = null;

var errcontainer = null;
var container = null;
var media_video = null;
//var media_audio = null;
var media_video_source = null;
//var media_audio_source = null;
var media_video_subtitles = null;
var selector1 = null;
var selector2 = null;
var selector3 = null;
var filterselector = null;
var nextbtn = null;
var filetagcontainer = null;
var shouldloop = 1;


let html_indx = 0;
let prev_html = [""];
let prev_tag = [0,0,0,0];
let next_direction = 1;
let current_media_id = 0;
let show_file_tags = true; // TODO
function throw_err(s){
	errcontainer.innerText = s;
	errcontainer.style.display = "block";
	throw Error(s);
}
function tagbtnclicked(e){
	selector1.value = (e.currentTarget.dataset.i|0) + 1;
	get_new_media();
}
function shuffle(array, preset_first_n_elements){ // Fisher-Yates (aka Knuth) Shuffle
	let currentIndex = preset_first_n_elements;
	let randomIndex;
	// While there remain elements to shuffle.
	const N = array.length;
	while(currentIndex < N){
		// Pick a remaining element.
		randomIndex = currentIndex + Math.floor(Math.random() * (N-currentIndex));
		// And swap it with the current element.
		[array[currentIndex], array[randomIndex]] = [array[randomIndex], array[currentIndex]];
		++currentIndex;
	}
	return array;
}
function render_tags(){
	for (let i = 0;  i < all_files.length;  ++i){
		if (all_files[i][0] === current_media_id){
			const tag_ids = all_files[i][2];
			let html = "";
			for (let tagid of tag_ids){
				html += `<button data-i="${tagid}">${all_tags_id2name[tagid]}</button>`;
			}
			filetagcontainer.innerHTML = html;
			for (let node of filetagcontainer.getElementsByTagName("button")){
				node.addEventListener("pointerup", tagbtnclicked);
			}
			break;
		}
	}
}
function rendermedia(){
	const ls = prev_html[html_indx];
	current_media_id = ls[0];
	const url = "/static/" + current_media_id.toString().padStart(4,'0') + "?v=" + global_version;
	if (ls[1] === "audio/m4a")
		ls[1] = "audio/mp4"; // Fix for licensing issues
	if (ls[1] === "video")
		ls[1] = "video/webm";
	if (ls[1] === "audio")
		ls[1] = "audio/webm";
	if (ls[1] === "img")
		ls[1] = "image/jpeg";
	if (ls[1] === "")
		ls[1] = "video/webm"; // Fallback
	if (ls[1].startsWith("image/")){
			container.style.background = `url(${url})`;
			media_video.style.display = "none";
			//media_audio.style.display = "none";
			media_video.pause();
			//media_audio.pause();
	/*} else if (ls[1].startsWith("audio/")){
			container.style.background = `url(${ls[2]})`;
			media_audio_source.type = ls[1];
			media_audio_source.src = url;
			media_audio.playbackRate = $$$audio_playbackRate;
			media_video.style.display = "none";
			media_audio.style.display = "unset";
			media_video.pause();
			media_audio.load();
			try {
				media_audio.play();
			} catch(e){
				console.log(e);
			}*/
	} else {
			/*for (let node of Array.from(media_video.getElementsByTagName("track"))){
				node.remove();
			}*/
			const useds = [];
			for (let i = 0;  i < ls[3].length;  ++i){
				const [srclang,src] = ls[3][i];
				useds.push(srclang);
				const caption_node = media_video_subtitles[srclang]; //document.createElement("track");
				//if (i === 0)
				//	caption_node.default = "";
				caption_node.src = src;
				//caption_node.srclang = srclang;
				//caption_node.kind = "subtitles";
				//caption_node.label = {"en":"English","de":"Deutsch"}[srclang];
				//media_video.appendChild(caption_node);
			}
			for (let i = 0;  i < media_video_subtitles.length;  ++i){
				if (!useds.includes(i))
					media_video_subtitles[i].src = "data:,";
			}
			
			media_video_source.setAttribute("type", ls[1]);
			media_video_source.setAttribute("src",  url);
			if (ls[1].startsWith("audio/")){
				const thumbnail = (typeof ls[2] === "number") ? tag2thumbnail[ls[2]] : ls[2];
				media_video.poster = thumbnail;
			} else {
				media_video.setAttribute("poster", "data:,"); // but "" also doesnt seem to trigger rerquest
			}
			media_video.playbackRate = $$$audio_playbackRate;
			media_video.style.display = "unset";
			//media_audio.style.display = "none";
			container.style.background = "unset";
			//media_audio.pause();
			media_video.load();
			try {
				media_video.play();
			} catch(e){
				console.log(e);
			}
	}
	if (show_file_tags){
		render_tags();
	}
}
function get_new_media(){
	next_direction = 1;
	const selector1_value = selector1.value|0;
	const selector2_value = selector2.value|0;
	const selector3_value = selector3.value|0;
	const filterselector_value = filterselector.value|0;
	
	document.location.hash = "#" + JSON.stringify([selector1_value,selector2_value,selector3_value,filterselector_value,shouldloop]);
	
	if ((prev_tag[0] !== selector1_value) || (prev_tag[1] !== selector2_value) || (prev_tag[2] !== selector3_value) || (prev_tag[3] !== filterselector_value)){
		prev_html.length = 1;
		html_indx = 0;
		prev_tag[0] = selector1_value;
		prev_tag[1] = selector2_value;
		prev_tag[2] = selector3_value;
		prev_tag[3] = filterselector_value;
	}
	if (html_indx === prev_html.length-1){
		const [tagid1_plus1,tagid2_plus1,NOT_tagid3_plus1,filterid] = prev_tag;
		const ls = all_tags_id2files[tagid1_plus1]
			.filter(x => ((    tagid2_plus1===0)||( all_tags_id2files[    tagid2_plus1].includes(x))))
			.filter(x => ((NOT_tagid3_plus1===0)||(!all_tags_id2files[NOT_tagid3_plus1].includes(x))))
			.filter(filterfn[filterid])
		;
		let ls_offset = 0;
		if (tagid1_plus1 !== 0){
			const intro_fileid = introduction_to_each_category[tagid1_plus1-1];
			if (intro_fileid && ls.includes(intro_fileid)){
				const indx = ls.indexOf(intro_fileid);
				[ls[0], ls[indx]] = [ls[indx], ls[0]];
				ls_offset = 1;
			}
		}
		shuffle(ls,ls_offset);
		if (ls.length === 0){
			throw_err("0 results");
		}
		for (let x of ls){
			if (x !== prev_html[html_indx]){
				for (let i = 0;  i < all_files.length;  ++i){
					if (all_files[i][0] === x){
						prev_html.push([all_files[i][0],all_files[i][3],all_files[i][1],all_files[i][4]]);
					}
				}
			}
		}
		if (html_indx !== prev_html.length-1){
			++html_indx;
			rendermedia();
		}
	} else if (html_indx !== prev_html.length-1){
		++html_indx;
		rendermedia();
	}
}
function get_prev_media(){
	if ((html_indx !== 1) && (html_indx !== 0)){
		--html_indx;
		rendermedia();
	}
}
function onmediaend(e){
	if (!e.currentTarget.loop)
		get_new_media();
}
let $$$audio_playbackRate = 1.0;
let $$$media_volume = 0.0;
function $$$media_rate_changed(eventobj){
	$$$audio_playbackRate = eventobj.target.playbackRate;
	/* Has to be done at start of each play() event, apparently
	if (eventobj.currentTarget === media_video){
		media_audio.playbackRate = $$$audio_playbackRate;
	} else {
		media_video.playbackRate = $$$audio_playbackRate;
	} */
}
function $$$media_volume_changed(eventobj){
	$$$media_volume = eventobj.target.volume;
	if (eventobj.currentTarget === media_video){
		// media_audio.volume = $$$media_volume;
	} else {
		media_video.volume = $$$media_volume;
	}
}

document.addEventListener('DOMContentLoaded', ()=>{
document.getElementById("randymediaplayer_prev").addEventListener("pointerup", ()=>{
	next_direction = -1;
	get_prev_media();
});
errcontainer = document.getElementById("randymediaplayer_errcontainer");
container = document.getElementById("randymediaplayer_container");
media_video = document.getElementById("randymediaplayer_media_video");
//media_audio = document.getElementById("randymediaplayer_media_audio");
media_video_source = document.getElementById("randymediaplayer_media_video_source");
//media_audio_source = document.getElementById("randymediaplayer_media_audio_source");
media_video_subtitles = document.getElementsByClassName("randymediaplayer_media_video_subtitles");
selector1 = document.getElementById("randymediaplayer_selector1");
selector2 = document.getElementById("randymediaplayer_selector2");
selector3 = document.getElementById("randymediaplayer_selector3");
filterselector = document.getElementById("randymediaplayer_filterselector");
nextbtn = document.getElementById("randymediaplayer_next");
filetagcontainer = document.getElementById("randymediaplayer_filetagcontainer");
errcontainer.addEventListener("pointerover", ()=>{
	errcontainer.style.display = "none";
});
nextbtn.addEventListener("pointerup", get_new_media);

media_video_source.addEventListener("error", get_new_media);
//media_audio_source.addEventListener("error", get_new_media);
media_video.addEventListener("ended", onmediaend);
media_video.addEventListener("ratechange", $$$media_rate_changed);
media_video.addEventListener("volumechange", $$$media_volume_changed);
//media_audio.addEventListener("ended", onmediaend);
//media_audio.addEventListener("ratechange", $$$media_rate_changed);
//media_audio.addEventListener("volumechange", $$$media_volume_changed);


getorpost("MACRO__ALL_FILES_JSON_PATH", null, datastr => {
	[all_tags_id2name,all_files,tag2thumbnail,introduction_to_each_category] = JSON.parse(datastr);
	for (let i = 0;  i < all_tags_id2name.length+1;  ++i){
		all_tags_id2files.push([]);
	}
	let s = "";
	for (let i = 0;  i < all_files.length;  ++i){
		const ls = all_files[i];
		const fileid = ls[0];
		all_files__as_dict[fileid] = ls;
		all_tags_id2files[0].push(fileid);
		for (let tagid of ls[2])
			all_tags_id2files[tagid+1].push(fileid);
		if (ls[2].length === 0)
			console.warn("No tagids:", fileid);
	}
	let optionshtml = '';
	let notoptionshtml = '';
	for (let tagid = 0;  tagid < all_tags_id2name.length;  ++tagid){
		const tagname = all_tags_id2name[tagid];
		optionshtml += `<option value="${tagid+1}">${tagname} [${all_tags_id2files[tagid+1].length}]</option>`;
		notoptionshtml += `<option value="${tagid+1}">NOT ${tagname}</option>`;
	}
	selector1.innerHTML = `<option value="0">Shuffle all [${all_tags_id2files[0].length}]</option>` + optionshtml;
	selector2.innerHTML = `<option value="0">(+Filter)</option>` + optionshtml;
	selector3.innerHTML = `<option value="0">(-Filter)</option>` + notoptionshtml;
	
	if (document.location.hash.length !== 0){
		[selector1.value, selector2.value, selector3.value, filterselector.value, shouldloop] = JSON.parse(document.location.hash.substr(1));
		if (shouldloop === 0){
			media_video.loop = false;
			//media_audio.loop = false;
		}
	}
	selector1.addEventListener("change", get_new_media);
	selector2.addEventListener("change", get_new_media);
	selector3.addEventListener("change", get_new_media);
	filterselector.addEventListener("change", get_new_media);
	
	get_new_media();
});
});
