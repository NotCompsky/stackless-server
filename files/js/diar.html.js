document.addEventListener("DOMContentLoaded", ()=>{
	const diarypagecontent = document.getElementById("diarypagecontent");
	const errmsgcontainer = document.getElementById("showerr_node");
	
	const diarybtn_prev = document.getElementById("diarybtn_prev");
	const diarybtn_next = document.getElementById("diarybtn_next");
	
	/*function id2int(id){
		let n = 0;
		for (let i = 0;  i < 4;  ++i){
			n *= 64;
			n += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_".indexOf(id[3-i]);
		}
		return n;
	}
	function int2id(n){
		let idstr = "";
		for (let i = 0;  i < 4;  ++i){
			idstr += "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"[n&63];
			n >>= 6;
		}
		return idstr;
	}*/
	
	function showdiarypage(id){
		if (id === "NEW"){
			// fake editing version
			
			errmsgcontainer.innerText = "Not authorised to create diary entries";
			errmsgcontainer.classList.remove("display-none");
			
			return;
		}
		fetch("/d00/"+id, {credentials:"include", mode:"no-cors", method:"GET"}).then(r => {
			if (r.ok){
				r.text().then(s => {
					diarybtn_prev.href = "#" + s.substr(0,4);
					diarybtn_next.href = "#" + s.substr(5,4);
					diarypagecontent.innerHTML = s.substr(10);
				});
				errmsgcontainer.classList.add("display-none");
			} else {
				errmsgcontainer.innerText = r.statusText;
				errmsgcontainer.classList.remove("display-none");
			}
		});
	}
	
	window.addEventListener("hashchange", e=>showdiarypage(e.newURL.substr(e.newURL.indexOf("#")+1)));
	
	if (document.location.hash.length > 1){
		showdiarypage(document.location.hash.substr(1));
	} else {
		document.location.hash = "#BAAA";
	}
});
