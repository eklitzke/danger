$(document).ready(function () {

	var newTd = (function (tr, text, className) {
		var td = document.createElement('td');
		if (text !== 0 && text !== "") {
			$(td).text(text);
		}
		if (className !== undefined) {
			td.className = className;
		}
		tr.appendChild(td);
	});

	var filterVisible = (function (s) {
		var r = new RegExp(s, "i");
		var library = document.getElementById("library");
		for (var i = 1; i < library.children.length; i++) {
			var elem = library.children[i];
			if (r.test(elem._search_name)) {
				elem.style.display = "";
			} else {
				elem.style.display = "none";
			}
		}
	});

	$('#searchbutton').click(function (e) {
		e.preventDefault();
		var searchbox = document.getElementById('searchbox');
		filterVisible(searchbox.value);
	});

	var audio_element = document.getElementById("aud");
	var updateLibrary = (function() {
		console.log("updateLibrary");
		$.get("/library", function (data) {
			var first = false;
			if (data === null) {
				console.log("library data was null");
				return;
			}
			console.log("got library with " + data.length + " tracks");
			var aud = document.getElementById("aud");
			var library = document.getElementById("library");
			for (var i = 0; i < data.length; i++) {
				var track = data[i];
				if (track.artist.length == 0 ||
					track.album.length == 0 ||
					track.title.length == 0) {
					console.log("skipping track");
					continue;
				}
				if (first === false) {
					if (!aud.src) {
						aud.src = "/music/" + track.name;
						aud.pause();
					}
				}
				var tr = document.createElement("tr");
				tr.className = "clickable library_tr";
				tr._track_name = track.name;
				tr._search_name = "" + track.title + " " + track.artist + " " + track.album;
				newTd(tr, track.tracknum, "track_tracknum");
				newTd(tr, track.title || track.name, "track_title");
				newTd(tr, track.artist, "track_artist");
				newTd(tr, track.album, "track_album");
				newTd(tr, track.year, "track_year");
				library.appendChild(tr);
			}
			$(".library_tr").click(function (e) {
				e.preventDefault();
				$('.playing').each(function (i, thing) {
					$(thing).removeClass('playing');
				});
				$(this).addClass('playing');
				aud.src = "/music/" + this._track_name;
				aud.play();
				console.log("calling play");
			});
			$("#loading_message").remove();
		});
	});

	updateLibrary();
});
