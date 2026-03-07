// Emscripten --js-library side file for FastLED WASM bindings.
//
// These functions were previously embedded via EM_ASM/EM_JS which caused
// wasm-dependent ASM_CONST offset tables in the JS glue output, making it
// impossible to cache the JS glue across sketch rebuilds.
//
// By moving them here, the JS glue is 100% deterministic for a given set of
// link flags, enabling fast incremental linking (wasm-ld only + cached JS).

addToLibrary({
  js_post_ui_elements: function(jsonStrPtr) {
    try {
      var jsonStr = UTF8ToString(jsonStrPtr);
      var uiElements = JSON.parse(jsonStr);
      postMessage({
        type: 'ui_elements_add',
        payload: { elements: uiElements }
      });
    } catch (error) {
      console.error('Error posting UI message from worker:', error);
    }
  },

  js_fetch_async: function(requestId, urlPtr) {
    var urlString = UTF8ToString(urlPtr);
    console.log('JavaScript fetch starting for request', requestId, 'URL:', urlString);
    fetch(urlString)
      .then(function(response) {
        console.log('Fetch response received for request', requestId, 'status:', response.status);
        if (!response.ok) {
          throw new Error('HTTP ' + response.status + ': ' + response.statusText);
        }
        return response.text();
      })
      .then(function(text) {
        console.log('Fetch text received for request', requestId, 'length:', text.length);
        Module._js_fetch_success_callback(requestId, stringToUTF8OnStack(text));
      })
      .catch(function(error) {
        console.error('Fetch error for request', requestId, ':', error.message);
        Module._js_fetch_error_callback(requestId, stringToUTF8OnStack(error.message));
      });
  },
});
