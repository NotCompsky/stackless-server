var wasm_binary = atob(
	// binary here, in base64
);
var wasm_bytes = new Uint8Array(wasm_binary.length);
for (let i = 0;  i < wasm_binary.length;  ++i){
	wasm_bytes[i] = wasm_binary.charCodeAt(i);
}



var wikitext2html = function(str){
	"<pre>" + str + "</pre>";
};







var wasm_module_buf;
var wikipagecontent;
var errmsgcontainer;
const text_encoder = new TextEncoder();
const text_decoder = new TextDecoder();
const thisurlhashstart = document.location.protocol + "//" + document.location.host + document.location.pathname + "#";

function showwikipage(title){
	fetch("/w00/"+title, {method:"GET"}).then(r => {
		if (r.ok){
			r.text().then(s => {
				wikipagecontent.innerHTML = wikitext2html(s);
				for (let node of wikipagecontent.getElementsByTagName("a")){
					const href = node.href;
					if (href.startsWith(thisurlhashstart) && href.includes("%20"))
						node.href = href.replaceAll("%20","_");
				}
			});
			errmsgcontainer.classList.add("display-none");
		} else {
			errmsgcontainer.innerText = r.statusText;
			errmsgcontainer.classList.remove("display-none");
		}
	});
}

document.addEventListener("DOMContentLoaded", ()=>{
	wikipagecontent = document.getElementById("wikipagecontent");
	errmsgcontainer = document.getElementById("showerr_node");
	
	window.addEventListener("hashchange", e=>showwikipage(e.newURL.substr(e.newURL.indexOf("#")+1)));
	WebAssembly.instantiate(wasm_bytes).then(wasm_module => {
		const exports = wasm_module.instance.exports;
		wasm_module_buf = new Uint8Array(exports.memory.buffer);
		wikitext2html = function(str){
			const strbuf = text_encoder.encode(str);
			const input_length = strbuf.length;
			wasm_module_buf.set(strbuf);
			const output_len = exports.wikitext2html(0, input_length, wasm_module_buf.length);
			return text_decoder.decode(wasm_module_buf.slice(input_length, input_length+output_len)).replaceAll("<a class=\"wikipage\" href=\"/wiki/", "<a class=\"wikipage\" href=\"#").replace(/[{][{]Infobox (([^}]|[}][^}]|[^>][}][}]|[^r]>[}][}]|[^b]r>[}][}]|[^<]br>[}][}])*)(<br>|\n)[}][}]/,"<details><summary>Infobox</summary><pre>$1</pre></details>");
		}
		if (document.location.hash.length > 1){
			showwikipage(document.location.hash.substr(1));
		}
	});
	document.getElementById("wikipagecustom").addEventListener("pointerup", ()=>{
		const text = prompt("Page title");
		if (text){
			const title = text.replaceAll(" ","_");
			document.location.hash = "#" + title;
		}
	});
	errmsgcontainer.addEventListener("pointerup", ()=>{
		errmsgcontainer.classList.add("display-none");
	});
});
