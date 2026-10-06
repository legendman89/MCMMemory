// Authored by Wuerfelhusten to support MCM bridge

#pragma once

#include <stdint.h>

#if defined(_WIN32)
#define MCM_HOST_CALL __cdecl
#else
#define MCM_HOST_CALL
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#pragma pack(push, 8)

	typedef uint64_t MCMHostContext;
	typedef uint32_t MCMHostResult;
	enum MCMHostResultValue
	{
		MCM_HOST_OK = 0,
		MCM_HOST_BUSY = 1,
		MCM_HOST_UNAVAILABLE = 2,
		MCM_HOST_INVALID_ARGUMENT = 3,
		MCM_HOST_DISPATCH_FAILED = 4,
		MCM_HOST_CANCELLED = 5,
		MCM_HOST_TIMED_OUT = 6,
		MCM_HOST_SESSION_INVALIDATED = 7,
		MCM_HOST_INVALID_DATA = 8
	};

	enum MCMHostValueType
	{
		MCM_HOST_INTEGER = 1,
		MCM_HOST_FLOAT = 2,
		MCM_HOST_STRING = 3
	};

	typedef struct MCMHostArgument
	{
		uint32_t type;
		int32_t integer;
		float number;
		const char *text;
	} MCMHostArgument;

	typedef struct MCMHostCall
	{
		const char *mcm_id;
		const char *function;
		const MCMHostArgument *arguments;
		uint32_t argument_count;
		uint32_t timeout_ms;
		uint32_t accept_confirmation;
	} MCMHostCall;

	// Runs once per accepted call on the game task queue; keep the receiver alive until it returns.
	// Reports execution only. The client checks and saves values; timeouts do not undo changes.
	typedef void(MCM_HOST_CALL *MCMHostCompletion)(void *a_user, MCMHostResult a_result, uint32_t a_confirmation_declined);

	enum MCMHostEventType
	{
		MCM_HOST_SESSION_BEGIN = 1,
		MCM_HOST_SESSION_END = 2,
		MCM_HOST_USER_CHANGE = 3,
		MCM_HOST_USER_CHANGING = 4,
		MCM_HOST_USER_REJECTED = 5
	};

	enum MCMHostControlType
	{
		MCM_HOST_TEXT = 1,
		MCM_HOST_TOGGLE = 2,
		MCM_HOST_SLIDER = 3,
		MCM_HOST_MENU = 4,
		MCM_HOST_COLOR = 5,
		MCM_HOST_KEYMAP = 6,
		MCM_HOST_INPUT = 7
	};

	// Copy retained data before returning. Names are raw, without aliases or translations.
	// CHANGING identifies the setting before its callback; record only CHANGE events.
	// Backup/restore emits no change events. Observers must not modify host state.
	typedef struct MCMHostEvent
	{
		uint64_t session;
		uint32_t type;
		uint32_t control_type;
		const char *mcm_id;
		const char *mod_name;
		const char *page_name;
		const char *label;
		const char *setting_id;
		const char *state_name;
		const char *value_text;
		int32_t page_index;
		int32_t option_index;
		uint32_t page_scoped_state;
		MCMHostArgument value;
		uint64_t change_id;
		// Requested action: 0 = set value, 1 = activate, 2 = reset.
		uint32_t intent;
		uint32_t confirmation_accepted;
		uint32_t confirmation_declined;
	} MCMHostEvent;

	typedef void(MCM_HOST_CALL *MCMHostObserver)(void *a_user, const MCMHostEvent *a_event);

	typedef struct MCMHostData MCMHostData;

	typedef struct MCMHostControlView
	{
		int32_t option_index;
		uint32_t type;
		uint32_t disabled;
		uint32_t hidden;
		const char *label;
		const char *state_name;
		const char *setting_id;
		MCMHostArgument value;
		uint32_t dialog_ready;
		// Slider: start, default, min, max, step. Color: start, default.
		float dialog_numbers[5];
		const char *dialog_text;
		int32_t menu_index;
		int32_t menu_default;
		uint32_t menu_count;
		const char *const *menu_items;
	} MCMHostControlView;

	typedef struct MCMHostPageView
	{
		const char *name;
		int32_t index;
		uint32_t controls_ready;
		uint32_t control_count;
		const MCMHostControlView *controls;
	} MCMHostPageView;

	typedef struct MCMHostModView
	{
		const char *mcm_id;
		const char *name;
		const char *script_name;
		const char *owner_plugin;
		uint32_t quest_form_id;
		uint32_t page_scoped_state;
		uint32_t navigation_ready;
		uint32_t page_count;
		const MCMHostPageView *pages;
	} MCMHostModView;

	typedef struct MCMHostDataView
	{
		uint64_t session;
		uint32_t mod_count;
		const MCMHostModView *mods;
	} MCMHostDataView;

	typedef struct MCMBridgeHost
	{
		// Use context/call functions on the game task queue, outside rendering.
		// invoke copies inputs before returning; rejected calls get no completion callback.
		// Contexts are exclusive and valid only for the current game session.
		uint32_t(MCM_HOST_CALL *is_ready)(void);
		MCMHostResult(MCM_HOST_CALL *begin_context)(const char *a_owner, uint32_t a_restore, MCMHostContext *a_context);
		MCMHostResult(MCM_HOST_CALL *invoke)(MCMHostContext a_context, const MCMHostCall *a_call, MCMHostCompletion a_completion, void *a_user);
		MCMHostResult(MCM_HOST_CALL *cancel_context)(MCMHostContext a_context);
		MCMHostResult(MCM_HOST_CALL *end_context)(MCMHostContext a_context);
		
		// Observers run on the game task queue and may unsubscribe themselves.
		// Unsubscribe before destroying the receiver. Subscriptions survive save changes; sessions do not.
		MCMHostResult(MCM_HOST_CALL *subscribe)(MCMHostObserver a_observer, void *a_user, uint64_t *a_subscription);
		MCMHostResult(MCM_HOST_CALL *unsubscribe)(uint64_t a_subscription);

		// Zero reads registration only; an idle, owned context adds current navigation, page and prepared dialog.
		// Reads run no script callbacks. Data stays valid until release_data, even across saves,
		// but grants no access to another context or session.
		MCMHostResult(MCM_HOST_CALL *acquire_data)(MCMHostContext a_context, MCMHostData **a_data, MCMHostDataView *a_view);
		void(MCM_HOST_CALL *release_data)(MCMHostData *a_data);
	} MCMBridgeHost;

	typedef const MCMBridgeHost *(MCM_HOST_CALL *MCMBridgeGetHost)(void);

#pragma pack(pop)

#ifdef __cplusplus
}
#endif
