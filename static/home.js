$(document).ready(function () {
	$.get("/library", function (data) {
		if (data === null)
			return;
		var tracks_ul = document.getElementById("tracks");
		for (var i = 0; i < data.length; i++) {
			var li = document.createElement("li");
			li.className = "clickable";
			var track = data[i];
			$(li).text(track);
			(function (track) {
				$(li).click(function (e) {
					$('.playing').each(function (i, thing) {
						$(thing).removeClass('playing');
					});
					$(this).addClass('playing');

					e.preventDefault();
					var audio_element = document.createElement("audio");
					audio_element.controls = "controls";
					audio_element.src = "/fetch/" + track;
					var cont = document.getElementById("audio_container");
					while (cont.children.length) {
						$(cont.children[0]).remove();
					}
					cont.appendChild(audio_element);
					audio_element.play();
				});
			})(track);
			tracks_ul.appendChild(li);
		}
		$("#loading_message").remove();
	});
});
