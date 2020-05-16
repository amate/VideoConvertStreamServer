
// トースト初期化
var toast_hideCount = 0;
$('.toast').toast();

var toastTopMargin = 10;

function PopupToast(leftPos, topPos, toastBody)
{
	// トースト表示位置を調節
	$('#toast_notify').css({
		'left': leftPos + 'px',
		'top': topPos + toastTopMargin + 'px'
	});
	$('#toast_title').text(toastBody);
	
	$('#toast_notify').toast('show');
	++toast_hideCount;
	let delay = $('#toast_notify').data('delay');
	setTimeout(() => {
		--toast_hideCount;
		if (toast_hideCount == 0) {
			$('#toast_notify').toast('hide');
		}
	}, delay);
}

function OperateVolumeAPI(event)
{
	operation = event.data.operation;
	let queryOperation = {
		operation: operation,
	};

	$.getJSON("/VolumeAPI", queryOperation)
	.done(function(json){
		// トースト表示位置を調節
		let buttonElm = $('#' + json.Operation);
		let leftPos = buttonElm.position().left;
		let topPos = buttonElm.position().top + buttonElm.height();

		let toastBody = "";
		//$('#toast_title').text(json.Operation);
		if (json.Operation == "toggle_mute") {
			toastBody = "NowMute: " + json.NowMute;
		} else {
			toastBody = "Volume: " + json.CurrentVlume;
		}
		PopupToast(leftPos, topPos, toastBody);
		
	})
	.fail(function(jqXHR, textStatus, errorThrown) {
		console.error("getJSON fail: " + textStatus);
	});
}

function PrevSleepAPI()
{
	$.getJSON("/PrevSleepAPI")
	.done(function(json){
		let buttonElm = $('#prev_sleep');
		let leftPos = buttonElm.position().left;
		let topPos = buttonElm.position().top + buttonElm.height();

		PopupToast(leftPos, topPos, "success!");
	})
	.fail(function(jqXHR, textStatus, errorThrown) {
		console.error("getJSON fail: " + textStatus);
	});
}

$(document).ready(function() {
	$('#prev_sleep').on('click', PrevSleepAPI);

	$('#toggle_mute').on('click', { operation: "toggle_mute"}, OperateVolumeAPI);

	$('#volume_up').on('click', { operation: "volume_up"}, OperateVolumeAPI);
	$('#volume_down').on('click', { operation: "volume_down"}, OperateVolumeAPI);

});

$.getJSON("/PlayHistoryAPI")
.done(function(json){
    for (let i = 0; i < json.PlayHistory.length; ++i) {
		let playHistory = json.PlayHistory[i];

		// icon
		let tdIcon = $('<td>');		
        tdIcon.append('<i class="fa fa-film  fa-fw" aria-hidden="true"></i>');

		// history
		let anchorURL = "/play/" + playHistory;
        anchorURL = anchorURL.replace(/#/g, "<hash>");// + location.search;
        let nameAnchor = $('<a>', {
            href: anchorURL
        }).text(playHistory);

        let tdName = $('<td>').append(nameAnchor);
        let tr = $('<tr>').append(tdIcon).append(tdName);
        $('#PlayHisotryTable').append(tr);
	
	}
	//console.log(json);
})
.fail(function(jqXHR, textStatus, errorThrown) {
	console.error("getJSON fail: " + textStatus);
});