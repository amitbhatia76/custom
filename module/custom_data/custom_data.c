
/*****************************************************************************/
/*                              Legal                                        */
/*****************************************************************************/

/*
** Copyright �2015-2017. Lantronix, Inc. All Rights Reserved.
** By using this software, you are agreeing to the terms of the Software
** Development Kit (SDK) License Agreement included in the distribution package
** for this software (the �License Agreement�).
** Under the License Agreement, this software may be used solely to create
** custom applications for use on the Lantronix xPico Wi-Fi product.
** THIS SOFTWARE AND ANY ACCOMPANYING DOCUMENTATION IS PROVIDED "AS IS".
** LANTRONIX SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED
** TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, NON-INFRINGEMENT AND FITNESS
** FOR A PARTICULAR PURPOSE.
** LANTRONIX HAS NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
** ENHANCEMENTS, OR MODIFICATIONS TO THIS SOFTWARE.
** IN NO EVENT SHALL LANTRONIX BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
** SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
** ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF
** LANTRONIX HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*****************************************************************************/
/*                           Documentation                                   */
/*****************************************************************************/

/*!
** \addtogroup example
** @{
*/

/*!
** \defgroup custom_data custom_data
** @{
**
** The \b custom_data module demonstrates how to define and access custom
** configurables, how to embed files in a module, and how to overlay files
** using embeded files.
**
** Build it from project "customDataDemo".
**
** After loading this project on your device, you will see a "Custom Data"
** tab at the left in the Web Manager.
** Hit that tab, and you will see a "Custom Device Configuration" page.
** Bring up the Trouble Log on a Line to watch what is happening.
** Change "Name", "Rank", and/or "Serial Number", hit submit, and
** see the changes come out on the Trouble Log.
*/

/*****************************************************************************/
/*                             Includes                                      */
/*****************************************************************************/

#include <stdio.h> /* MinGW plus Lantronix changes, delivered with SDK. */
#include <string.h> /* MinGW, delivered with SDK. */

#include "custom_data_module_defs.h" /* Automatically generated by make. */
#include "ltrx_compile_defines.h" /* Delivered with SDK. */
#include "ltrx_stream.h" /* Delivered with SDK. */
#include "ltrx_tlog.h" /* Delivered with SDK. */
#include "user_data_module_libs.h" /* Delivered with SDK. */

/*****************************************************************************/
/*                              Globals                                      */
/*****************************************************************************/

const struct user_data_external_functions *g_user_dataExternalFunctionEntry_pointer;

/*****************************************************************************/
/*                         Local Constants                                   */
/*****************************************************************************/

static const uint32_t s_delayTimeInMilliseconds = 500;

static const struct xml_emit_value_specification s_status =
{
	.type = XML_EMIT_VALUE_TYPE__STATUS,
	.groupName = "Custom",
	.itemName = "Changes"
};

/*****************************************************************************/
/*                         Local Variables                                   */
/*****************************************************************************/

static struct vardef_values_custom s_currentValues;

/*****************************************************************************/
/*                               Code                                        */
/*****************************************************************************/

static bool getXmlValue(
    const struct xml_emit_value_specification *xevs,
    char *buffer,
    size_t size
)
{
    struct output_stream_to_buffer osb;
    ltrx_output_stream_init_to_buffer(
		&osb, buffer, size, OUTPUT_STREAM_TO_BUFFER_MODE__ZERO_TERMINATE
	);
    return(ltrx_xml_emit_value(xevs, &osb.outStream));
}

static bool getItem(
    const char *desiredItemName,
    char *buffer,
    size_t size
)
{
    for(unsigned int i = 0; i < MAX_USER_DATA_CUSTOM_ITEMS; ++i)
    {
        const struct vardef_values_custom_item *vvci = &s_currentValues.item[i];
        if(strcmp(desiredItemName, vvci->instance) == 0)
        {
            snprintf(buffer, size, "%s", vvci->value);
            return true;
        }
    }
    return false;
}

static void customConfigMonitorThread(void *opaque)
{
    (void)opaque;
    char lastChangeValue[15] = "";
    getXmlValue(&s_status, lastChangeValue, sizeof(lastChangeValue));
    while(true)
    {
        char buffer[VARDEF_ELEMENT_ITEM_CUSTOM_ITEM_VALUE_STRLEN_MAX + 1];
        if(
            getXmlValue(&s_status, buffer, sizeof(buffer)) &&
            strcmp(buffer, lastChangeValue) != 0 &&
            ltrx_user_data_lookup_group("Device", &s_currentValues)
        )
        {
            /*
            ** The change number is different.
            ** This means there can be new configurable values ready.
            ** Save the new change number for future reference.
            */
            snprintf(lastChangeValue, sizeof(lastChangeValue), "%s", buffer);
            TLOG(
                TLOG_SEVERITY_LEVEL__INFORMATIONAL,
                "Change %s:", lastChangeValue
            );
            if(getItem("Name", buffer, sizeof(buffer)))
            {
                TLOG(
                    TLOG_SEVERITY_LEVEL__INFORMATIONAL,
                    "  Name: %s", buffer
                );
            }
            if(getItem("Rank", buffer, sizeof(buffer)))
            {
                TLOG(
                    TLOG_SEVERITY_LEVEL__INFORMATIONAL,
                    "  Rank: %s", buffer
                );
            }
            if(getItem("Serial Number", buffer, sizeof(buffer)))
            {
                TLOG(
                    TLOG_SEVERITY_LEVEL__INFORMATIONAL,
                    "  Serial Number: %s", buffer
                );
            }
        }
        ltrx_thread_sleep(s_delayTimeInMilliseconds);
    }
}

void custom_data_module_registration(void)
{
    ltrx_module_register(&g_custom_dataModuleInfo);
}

void custom_data_module_startup(void)
{
    g_user_dataExternalFunctionEntry_pointer = ltrx_module_functions_lookup("User_Data");
    ltrx_thread_create(
        "Custom Config Monitor",
        customConfigMonitorThread,
        NULL,
        4000
    );
}

/*!
** @}
*/

/*!
** @}
*/