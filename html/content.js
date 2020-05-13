
function operation_click(event)
{
	operation = event.data.operation;
	
    let operationPath = "/"+ operation;
	$.get(operationPath
    , function(data) {
		//$('#modal_title').text(data);
		//$('#exampleModal').modal();
        $('#toast_notify').toast('show');
	});
}

$('.toast').toast();

$(document).ready(function() {
	$('#prev_sleep').on('click', { operation: "prev_sleep"}, operation_click);
	$('#toggle_mute').on('click', { operation: "toggle_mute"}, operation_click);

	$('#VolumeUp').on('click', { operation: "VolumeUp"}, operation_click);
	$('#VolumeDown').on('click', { operation: "VolumeDown"}, operation_click);

});