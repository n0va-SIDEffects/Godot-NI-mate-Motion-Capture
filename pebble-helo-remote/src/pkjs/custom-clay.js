// Laeuft innerhalb der Clay-Konfigurationsseite: der Spenden-Button oeffnet
// die Buy-me-a-coffee-Seite im Browser des Handys.
module.exports = function(minified) {
  var clayConfig = this;
  var URL = 'https://buymeacoffee.com/SIDEffects';

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var button = clayConfig.getItemById('donate');
    if (!button) return;
    button.on('click', function() {
      var w = null;
      try { w = window.open(URL, '_blank'); } catch (e) {}
      if (!w) { window.location.href = URL; }
    });
  });
};
