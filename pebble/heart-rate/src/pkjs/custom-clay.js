/*
 * Runs inside the settings page itself. Clay has no link item, so the donate button opens the
 * address by hand, falling back to the same window when a pop-up is blocked.
 */
module.exports = function() {
  var clayConfig = this;
  var url = require('./config').DONATION_URL;

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var button = clayConfig.getItemById('donate');
    if (!button || !url) {
      return;
    }
    button.on('click', function() {
      var opened = null;
      try {
        opened = window.open(url, '_blank');
      } catch (e) {
        opened = null;
      }
      if (!opened) {
        window.location.href = url;
      }
    });
  });
};
