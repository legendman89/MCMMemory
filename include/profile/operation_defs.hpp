#pragma once

#define FOREACH_OPERATION_MODE(MODE) \
    MODE(Automatic) \
    MODE(Manual)

#define FOREACH_OPERATION_RESULT(RESULT) \
    RESULT(Completed) \
    RESULT(Failed) \
    RESULT(Cancelled)
