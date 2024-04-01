const $=document.getElementById.bind(document),
	global_version=MACRO__GLOBAL_VERSION,
	b=(a,b)=>a.startsWith(b), // saves 9 characters each use
	p=(a,b)=>a.push(b), // saves 3 characters each use
	l=a=>a.length, // used 12 times; saves 4 characters each use
	e=(a,f)=>a.addEventListener("pointerup",f), // saves 15 chars each use
	o=(i,s)=>`<option value="${i}">${s}</option>`,
	filterfn=[
		function(fileid){ return true; },
		function(fileid){ return b(all_files__as_dict[fileid][3],"image/"); },
		function(fileid){ return all_files__as_dict[fileid][3] === "image/gif"; },
		function(fileid){ console.log(fileid, all_files__as_dict[fileid]); return b(all_files__as_dict[fileid][3],"video/"); },
		function(fileid){ return b(all_files__as_dict[fileid][3],"audio/"); },
		function(fileid){ return b(all_files__as_dict[fileid][3],"video/") || b(all_files__as_dict[fileid][3],"audio/"); },
		function(fileid){ return b(all_files__as_dict[fileid][3],"video/") || all_files__as_dict[fileid][3]==="image/gif"; }
	];


//var media_audio = null;
//var media_audio_source = null;


let all_tags_id2name=null,
	all_files=null,
	all_files__as_dict={},
	all_tags_id2files=[],
	tag2thumbnail=null,
	introduction_to_each_category=null,
	errcontainer=null,
	container=null,
	media_video=null,
	media_video_source=null,
	media_video_subtitles=null,
	selector1=null,
	selector2=null,
	selector3=null,
	filterselector=null,
	nextbtn=null,
	filetagcontainer=null,
	shouldloop=1,
	$$$audio_playbackRate=1.0,
	$$$media_volume=0.0,
	html_indx=0,
	prev_html=[""],
	prev_tag=[0,0,0,0],
	next_direction=1,
	current_media_id=0;

function tagbtnclicked(e){
	selector1.value = (e.currentTarget.dataset.i|0) + 1;
	get_new_media();
}
function shuffle(array, preset_first_n_elements){ // Fisher-Yates (aka Knuth) Shuffle
	let currentIndex=preset_first_n_elements,randomIndex,N=l(array);
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
	for (let i = 0;  i < l(all_files);  ++i){
		if (all_files[i][0] === current_media_id){
			let html = "";
			for (let tagid of all_files[i][2]){
				html += `<button data-i="${tagid}">${all_tags_id2name[tagid]}</button>`;
			}
			filetagcontainer.innerHTML = html;
			for (let node of filetagcontainer.getElementsByTagName("button")){
				e(node,"pointerup",tagbtnclicked);
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
	if (b(ls[1],"image/")){
			container.style.background = `url(${url})`;
			media_video.style.display = "none";
			//media_audio.style.display = "none";
			media_video.pause();
			//media_audio.pause();
	/*} else if (b(ls[1],"audio/")){
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
				p(useds,srclang);
				const caption_node = media_video_subtitles[srclang]; //document.createElement("track");
				//if (i === 0)
				//	caption_node.default = "";
				caption_node.src = src;
				//caption_node.srclang = srclang;
				//caption_node.kind = "subtitles";
				//caption_node.label = {"en":"English","de":"Deutsch"}[srclang];
				//media_video.appendChild(caption_node);
			}
			for (let i = 0;  i < l(media_video_subtitles);  ++i){
				if (!useds.includes(i))
					media_video_subtitles[i].src = "data:,";
			}
			
			media_video_source.setAttribute("type", ls[1]);
			media_video_source.setAttribute("src",  url);
			if (b(ls[1],"audio/")){
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
	render_tags();
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
	if (html_indx === l(prev_html)-1){
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
		if (l(ls) === 0){
			errcontainer.innerText = "0 results";
			errcontainer.style.display = "block";
			return;
		}
		for (let x of ls){
			if (x !== prev_html[html_indx]){
				for (let i = 0;  i < l(all_files);  ++i){
					if (all_files[i][0] === x){
						p(prev_html,[all_files[i][0],all_files[i][3],all_files[i][1],all_files[i][4]]);
					}
				}
			}
		}
		if (html_indx !== l(prev_html)-1){
			++html_indx;
			rendermedia();
		}
	} else if (html_indx !== l(prev_html)-1){
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
$("randymediaplayer_prev").addEventListener("pointerup", ()=>{
	next_direction = -1;
	get_prev_media();
});
errcontainer = $("randymediaplayer_errcontainer");
container = $("randymediaplayer_container");
media_video = $("randymediaplayer_media_video");
//media_audio = $("randymediaplayer_media_audio");
media_video_source = $("randymediaplayer_media_video_source");
//media_audio_source = $("randymediaplayer_media_audio_source");
media_video_subtitles = document.getElementsByClassName("randymediaplayer_media_video_subtitles");
selector1 = $("randymediaplayer_selector1");
selector2 = $("randymediaplayer_selector2");
selector3 = $("randymediaplayer_selector3");
filterselector = $("randymediaplayer_filterselector");
nextbtn = $("randymediaplayer_next");
filetagcontainer = $("randymediaplayer_filetagcontainer");
e(errcontainer,"pointerover",()=>{
	errcontainer.style.display = "none";
});
e(nextbtn,"pointerup",get_new_media);

e(media_video_source,"error",get_new_media);
//e(media_audio_source,"error",get_new_media);
e(media_video,"ended",onmediaend);
e(media_video,"ratechange",$$$media_rate_changed);
e(media_video,"volumechange",$$$media_volume_changed);
//e(media_audio,"ended",onmediaend);
//e(media_audio,"ratechange",$$$media_rate_changed);
//e(media_audio,"volumechange",$$$media_volume_changed);

fetch("MACRO__ALL_FILES_JSON_PATH", {credentials:"include", mode:"no-cors", method:"GET"}).then(r => {
	if (!r.ok){
		showerr(`Server returned ${r.status}: ${r.statusText}`);
	}
	r.text().then(datastr => {
		[all_tags_id2name,all_files,tag2thumbnail,introduction_to_each_category] = JSON.parse(datastr);
		let s="",h=document.location.hash,optionshtml="",notoptionshtml="",i=0;
		for (;  i < l(all_tags_id2name)+1;  ++i){
			p(all_tags_id2files,[]);
		}
		for (i = 0;  i < l(all_files);  ++i){
			const ls=all_files[i],fileid=ls[0];
			all_files__as_dict[fileid] = ls;
			p(all_tags_id2files[0],fileid);
			for (let tagid of ls[2])
				p(all_tags_id2files[tagid+1],fileid);
			if (ls[2].length === 0)
				console.warn("No tagids:", fileid);
		}
		for (i = 0;  i < l(all_tags_id2name);  ++i){ // i === tagid
			const tagname = all_tags_id2name[i];
			optionshtml += o(i+1,`${tagname} [${all_tags_id2files[i+1].length}]`);
			notoptionshtml += o(i+1,`NOT ${tagname}`);
		}
		selector1.innerHTML = o(0,`Shuffle all [${all_tags_id2files[0].length}]`) + optionshtml;
		selector2.innerHTML = o(0,"(+Filter)") + optionshtml;
		selector3.innerHTML = o(0,"(-Filter)") + notoptionshtml;
		
		if (l(h) !== 0){
			[selector1.value, selector2.value, selector3.value, filterselector.value, shouldloop] = JSON.parse(h.substr(1));
			if (shouldloop === 0){
				media_video.loop = false;
				//media_audio.loop = false;
			}
		}
		e(selector1,"change",get_new_media);
		e(selector2,"change",get_new_media);
		e(selector3,"change",get_new_media);
		e(filterselector,"change",get_new_media);
		
		get_new_media();
	});
});
