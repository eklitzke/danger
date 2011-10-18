$(document).ready(function () {

	var newTd = (function (tr, text, className) {
		var t = text || "";
		var td = document.createElement('td');
		if (t.length) {
			$(td).text(text || "");
		}
		if (className !== undefined) {
			td.className = className;
		}
		tr.appendChild(td);
	});

	var updateLibrary = (function() {
		console.log("updateLibrary");
		$.get("/library", function (data) {
			if (data === null) {
				console.log("library data was null");
				return;
			}
			console.log("got library with " + data.length + " tracks");
			var library = document.getElementById("library");
			for (var i = 0; i < data.length; i++) {
				var track = data[i];
				if (track.title.length == 0) {
					console.log("skipping track without title");
					continue;
				}
				var tr = document.createElement("tr");
				tr.className = "clickable library_tr";
				tr._track_name = track.name;
				newTd(tr, track.tracknum);
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
				var audio_element = document.createElement("audio");
				audio_element.controls = "controls";
				audio_element.src = "/fetch/" + this._track_name;
				var cont = document.getElementById("audio_container");
				while (cont.children.length) {
					$(cont.children[0]).remove();
				}
				cont.appendChild(audio_element);
				audio_element.play();
			});
			$("#loading_message").remove();
		});
	});

	updateLibrary();
});
