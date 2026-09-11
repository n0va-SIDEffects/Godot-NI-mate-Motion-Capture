#include "dictation.h"
#include "i18n.h"
#include "list_window.h"
#include "model.h"
#include "status_window.h"

#ifdef PBL_MICROPHONE

static DictationSession *s_session;
static char s_text[DESC_LEN];

static void prv_dictation_callback(DictationSession *session, DictationSessionStatus status,
                                   char *transcription, void *context) {
  if (status == DictationSessionStatusSuccess && transcription && transcription[0]) {
    strncpy(s_text, transcription, DESC_LEN - 1);
    s_text[DESC_LEN - 1] = '\0';
    list_window_push_for_text(s_text);      // now pick the project
    return;
  }
  switch (status) {
    case DictationSessionStatusFailureConnectivityError:
      model_set_message(STR(S_DICT_NO_CONNECTION), true);
      break;
    case DictationSessionStatusFailureDisabled:
      model_set_message(STR(S_DICT_DISABLED), true);
      break;
    case DictationSessionStatusFailureNoSpeechDetected:
      model_set_message(STR(S_DICT_NOTHING), false);
      break;
    default:
      model_set_message(STR(S_DICT_CANCELLED), false);
      break;
  }
  status_window_refresh();
}

void dictation_start(void) {
  if (!s_session) {
    s_session = dictation_session_create(DESC_LEN, prv_dictation_callback, NULL);
    if (!s_session) {
      model_set_message(STR(S_DICT_UNAVAILABLE), true);
      status_window_refresh();
      return;
    }
    dictation_session_enable_confirmation(s_session, true);
    dictation_session_enable_error_dialogs(s_session, true);
  }
  dictation_session_start(s_session);
}

void dictation_deinit(void) {
  if (s_session) {
    dictation_session_destroy(s_session);
    s_session = NULL;
  }
}

#else

void dictation_start(void) {
  model_set_message(STR(S_NO_MICROPHONE), true);
  status_window_refresh();
}

void dictation_deinit(void) {}

#endif
