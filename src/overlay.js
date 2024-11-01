function js_DisplayOverlay() {
    overlay_show();
}

function overlay_show() {
    document.getElementById("overlay").style.display = "block";
}

function overlay_close() {
    document.getElementById("overlay").style.display = "none";
}

function overlay_share_build() {
    var build = UTF8ToString(_generate_build());
    if (build.length == 0) {
        alert("No build loaded");
        return;
    }

    var xhttp = new XMLHttpRequest();
    xhttp.onreadystatechange = function() {
        if (this.readyState == 4) {
            if (this.status == 200) {
                var obj = JSON.parse(this.responseText);
                window.open("/share-link.html?link=" + obj.url, '_blank');
            } else {
                alert("Share server errored out");
            }
        }
    };
    xhttp.open("POST", "/kv/put?ver=" + CURRENT_VERSION, true);
    xhttp.send(build);
}
