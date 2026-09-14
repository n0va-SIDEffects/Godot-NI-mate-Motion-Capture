/*
 * Runs inside the settings page. Clay turns this function into text and drops it into the page, so
 * it cannot require anything or reach anything outside itself: every value it needs must be
 * written out here. An earlier version read the address from config.js, which left a require call
 * in the page and stopped it from loading at all.
 *
 * Keep the address below in step with DONATION_URL in config.js.
 */
module.exports = function() {
  var clayConfig = this;
  var url = 'https://buymeacoffee.com/SIDEffects';

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var button = clayConfig.getItemById('donate');
    if (!button) {
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
