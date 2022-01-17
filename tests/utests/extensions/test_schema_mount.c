/**
 * @file test_schema_mount.c
 * @author Tadeas Vintrlik <xvintr04@stud.fit.vutbr.cz>
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief unit tests for Schema Mount extension support
 *
 * Copyright (c) 2021 - 2022 CESNET, z.s.p.o.
 *
 * This source code is licensed under BSD 3-Clause License (the "License").
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://opensource.org/licenses/BSD-3-Clause
 */

#define _UTEST_MAIN_
#include "utests.h"

#include "libyang.h"

void **glob_state;

static int
setup(void **state)
{
    const char *schema =
            "module sm {yang-version 1.1;namespace \"urn:sm\";prefix \"sm\";"
            "import ietf-yang-schema-mount {prefix yangmnt;}"
            "import ietf-interfaces {prefix if;}"
            "container root {yangmnt:mount-point \"root\";}"
            "container root2 {yangmnt:mount-point \"root\";}"
            "container root3 {"
            "  list ls { key name; leaf name {type string;}"
            "    container mnt-root {yangmnt:mount-point \"mnt-root\";}"
            "  }"
            "}"
            "leaf target{type string;}"
            "augment /if:interfaces/if:interface {"
            "  leaf sm-name {type leafref {path \"/sm:target\";}}"
            "}"
            "}";

    UTEST_SETUP;
    glob_state = state;

    assert_int_equal(LY_SUCCESS, ly_ctx_set_searchdir(UTEST_LYCTX, TESTS_DIR_MODULES_YANG));
    assert_int_equal(LY_SUCCESS, lys_parse_mem(UTEST_LYCTX, schema, LYS_IN_YANG, NULL));
    assert_non_null(ly_ctx_load_module(UTEST_LYCTX, "iana-if-type", NULL, NULL));

    return 0;
}

static void
test_schema(void **state)
{
    struct lys_module *mod;
    const char *schema;
    char *str;

    /* invalid */
    schema =
            "module sm {\n"
            "  namespace \"urn:sm\";\n"
            "  prefix sm;\n"
            "\n"
            "  import ietf-yang-schema-mount {\n"
            "    prefix yangmnt;\n"
            "  }\n"
            "\n"
            "  container root {\n"
            "    yangmnt:mount-point \"root\";\n"
            "  }\n"
            "}\n";
    assert_int_equal(LY_EINVAL, lys_parse_mem(UTEST_LYCTX, schema, LYS_IN_YANG, NULL));
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - Schema Mount, version 1\": "
            "Extension \"yangmnt:mount-point\" instance not allowed in YANG version 1 module.",
            "/sm:root/{extension='yangmnt:mount-point'}/root");

    schema =
            "module sm {\n"
            "  yang-version 1.1;\n"
            "  namespace \"urn:sm\";\n"
            "  prefix sm;\n"
            "\n"
            "  import ietf-yang-schema-mount {\n"
            "    prefix yangmnt;\n"
            "  }\n"
            "\n"
            "  yangmnt:mount-point \"root\";\n"
            "}\n";
    assert_int_equal(LY_EINVAL, lys_parse_mem(UTEST_LYCTX, schema, LYS_IN_YANG, NULL));
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - Schema Mount, version 1\": "
            "Extension \"yangmnt:mount-point\" instance allowed only in container or list statement.",
            "/sm:{extension='yangmnt:mount-point'}/root");

    schema =
            "module sm {\n"
            "  yang-version 1.1;\n"
            "  namespace \"urn:sm\";\n"
            "  prefix sm;\n"
            "\n"
            "  import ietf-yang-schema-mount {\n"
            "    prefix yangmnt;\n"
            "  }\n"
            "\n"
            "  container root {\n"
            "    leaf l {\n"
            "      type empty;\n"
            "      yangmnt:mount-point \"root\";\n"
            "    }\n"
            "  }\n"
            "}\n";
    assert_int_equal(LY_EINVAL, lys_parse_mem(UTEST_LYCTX, schema, LYS_IN_YANG, NULL));
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - Schema Mount, version 1\": "
            "Extension \"yangmnt:mount-point\" instance allowed only in container or list statement.",
            "/sm:root/l/{extension='yangmnt:mount-point'}/root");

    schema =
            "module sm {\n"
            "  yang-version 1.1;\n"
            "  namespace \"urn:sm\";\n"
            "  prefix sm;\n"
            "\n"
            "  import ietf-yang-schema-mount {\n"
            "    prefix yangmnt;\n"
            "  }\n"
            "\n"
            "  list l {\n"
            "    key \"k\";\n"
            "    leaf k {\n"
            "      type string;\n"
            "    }\n"
            "    yangmnt:mount-point \"root\";\n"
            "    yangmnt:mount-point \"root2\";\n"
            "  }\n"
            "}\n";
    assert_int_equal(LY_EINVAL, lys_parse_mem(UTEST_LYCTX, schema, LYS_IN_YANG, NULL));
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - Schema Mount, version 1\": "
            "Multiple extension \"yangmnt:mount-point\" instances.",
            "/sm:l/{extension='yangmnt:mount-point'}/root2");

    /* valid */
    schema =
            "module sm {\n"
            "  yang-version 1.1;\n"
            "  namespace \"urn:sm\";\n"
            "  prefix sm;\n"
            "\n"
            "  import ietf-yang-schema-mount {\n"
            "    prefix yangmnt;\n"
            "  }\n"
            "\n"
            "  container root {\n"
            "    yangmnt:mount-point \"root\";\n"
            "  }\n"
            "}\n";
    assert_int_equal(LY_SUCCESS, lys_parse_mem(UTEST_LYCTX, schema, LYS_IN_YANG, &mod));
    lys_print_mem(&str, mod, LYS_YIN, 0);
    assert_string_equal(str, schema);
    free(str);
}

static LY_ERR
test_ext_data_clb(const struct lysc_ext_instance *ext, void *user_data, void **ext_data, ly_bool *ext_data_free)
{
    void **state = glob_state;
    struct lyd_node *data = NULL;

    (void)ext;

    if (user_data) {
        CHECK_PARSE_LYD_PARAM(user_data, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_SUCCESS, data);
    }

    *ext_data = data;
    *ext_data_free = 1;
    return LY_SUCCESS;
}

static void
test_parse_xml_invalid(void **state)
{
    const char *xml;
    struct lyd_node *data;

    /* no callback set */
    xml =
            "<root xmlns=\"urn:sm\">"
            "  <unknown xmlns=\"unknown\">"
            "    <interface>"
            "      <name>bu</name>"
            "      <type xmlns:ii=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ii:ethernetCsmacd</type>"
            "    </interface>"
            "  </unknown>"
            "</root>";
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, 0, LYD_VALIDATE_PRESENT, LY_EINVAL, data);
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - Schema Mount, version 1\": Failed to get extension data, no callback set.",
            NULL);

    /* unknown data */
    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb, NULL);
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, 0, LYD_VALIDATE_PRESENT, LY_SUCCESS, data);
    assert_string_equal(LYD_NAME(data), "root");
    assert_null(lyd_child(data));
    assert_non_null(data->next);
    assert_true(data->next->flags & LYD_DEFAULT);
    lyd_free_siblings(data);

    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_EVALID, data);
    CHECK_LOG_CTX("No module with namespace \"unknown\" in the context.",
            "Schema location /sm:root, data location /sm:root, line number 1.");

    /* missing required callback data */
    xml =
            "<root xmlns=\"urn:sm\">"
            "  <interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">"
            "    <interface>"
            "      <name>bu</name>"
            "    </interface>"
            "  </interfaces>"
            "</root>";
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_EVALID, data);
    CHECK_LOG_CTX("Node \"interfaces\" not found as a child of \"root\" node.",
            "Schema location /sm:root, data location /sm:root, line number 1.");

    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb,
            "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\" "
            "    xmlns:ds=\"urn:ietf:params:xml:ns:yang:ietf-datastores\">"
            "  <module-set>"
            "    <name>test-set</name>"
            "    <module>"
            "      <name>ietf-yang-library</name>"
            "      <revision>2019-01-04</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-library</namespace>"
            "    </module>"
            "  </module-set>"
            "  <schema>"
            "    <name>test-schema</name>"
            "    <module-set>test-set</module-set>"
            "  </schema>"
            "  <datastore>"
            "    <name>ds:running</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <datastore>"
            "    <name>ds:operational</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <content-id>1</content-id>"
            "</yang-library>"
            "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
            "  <module-set-id>1</module-set-id>"
            "</modules-state>");
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_EVALID, data);
    CHECK_LOG_CTX("Node \"interfaces\" not found as a child of \"root\" node.", NULL);

    /* missing module in yang-library data */
    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb,
            "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\" "
            "    xmlns:ds=\"urn:ietf:params:xml:ns:yang:ietf-datastores\">"
            "  <module-set>"
            "    <name>test-set</name>"
            "    <module>"
            "      <name>ietf-yang-library</name>"
            "      <revision>2019-01-04</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-library</namespace>"
            "    </module>"
            "  </module-set>"
            "  <schema>"
            "    <name>test-schema</name>"
            "    <module-set>test-set</module-set>"
            "  </schema>"
            "  <datastore>"
            "    <name>ds:running</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <datastore>"
            "    <name>ds:operational</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <content-id>1</content-id>"
            "</yang-library>"
            "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
            "  <module-set-id>1</module-set-id>"
            "</modules-state>"
            "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
            "  <mount-point>"
            "    <module>sm</module>"
            "    <label>root</label>"
            "    <inline/>"
            "  </mount-point>"
            "</schema-mounts>");
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_EVALID, data);
    CHECK_LOG_CTX("Node \"interfaces\" not found as a child of \"root\" node.", NULL);

    /* callback data correct, invalid YANG data */
    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb,
            "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\" "
            "    xmlns:ds=\"urn:ietf:params:xml:ns:yang:ietf-datastores\">"
            "  <module-set>"
            "    <name>test-set</name>"
            "    <module>"
            "      <name>ietf-datastores</name>"
            "      <revision>2018-02-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-datastores</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-library</name>"
            "      <revision>2019-01-04</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-library</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-schema-mount</name>"
            "      <revision>2019-01-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-interfaces</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-interfaces</namespace>"
            "    </module>"
            "    <module>"
            "      <name>iana-if-type</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:iana-if-type</namespace>"
            "    </module>"
            "    <import-only-module>"
            "      <name>ietf-yang-types</name>"
            "      <revision>2013-07-15</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-types</namespace>"
            "    </import-only-module>"
            "  </module-set>"
            "  <schema>"
            "    <name>test-schema</name>"
            "    <module-set>test-set</module-set>"
            "  </schema>"
            "  <datastore>"
            "    <name>ds:running</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <datastore>"
            "    <name>ds:operational</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <content-id>1</content-id>"
            "</yang-library>"
            "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
            "  <module-set-id>1</module-set-id>"
            "</modules-state>"
            "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
            "  <mount-point>"
            "    <module>sm</module>"
            "    <label>root</label>"
            "    <inline/>"
            "  </mount-point>"
            "</schema-mounts>");
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_EVALID, data);
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - Schema Mount, version 1\": "
            "Mandatory node \"type\" instance does not exist.",
            "Schema location /ietf-interfaces:interfaces/interface/type.");

    /* validation fail */
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT | LYD_PARSE_ONLY, 0, LY_SUCCESS, data);
    assert_int_equal(LY_EVALID, lyd_validate_all(&data, NULL, LYD_VALIDATE_PRESENT, NULL));
    lyd_free_siblings(data);

    /* success */
    xml =
            "<root xmlns=\"urn:sm\">\n"
            "  <interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>bu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "    </interface>\n"
            "  </interfaces>\n"
            "</root>\n";
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_SUCCESS, data);
    CHECK_LYD_STRING_PARAM(data, xml, LYD_XML, LYD_PRINT_WITHSIBLINGS);
    lyd_free_siblings(data);
}

static void
test_parse_xml_inline(void **state)
{
    const char *xml;
    struct lyd_node *data;

    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb,
            "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\" "
            "    xmlns:ds=\"urn:ietf:params:xml:ns:yang:ietf-datastores\">"
            "  <module-set>"
            "    <name>test-set</name>"
            "    <module>"
            "      <name>ietf-datastores</name>"
            "      <revision>2018-02-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-datastores</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-library</name>"
            "      <revision>2019-01-04</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-library</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-schema-mount</name>"
            "      <revision>2019-01-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-interfaces</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-interfaces</namespace>"
            "    </module>"
            "    <module>"
            "      <name>iana-if-type</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:iana-if-type</namespace>"
            "    </module>"
            "    <import-only-module>"
            "      <name>ietf-yang-types</name>"
            "      <revision>2013-07-15</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-types</namespace>"
            "    </import-only-module>"
            "  </module-set>"
            "  <schema>"
            "    <name>test-schema</name>"
            "    <module-set>test-set</module-set>"
            "  </schema>"
            "  <datastore>"
            "    <name>ds:running</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <datastore>"
            "    <name>ds:operational</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <content-id>1</content-id>"
            "</yang-library>"
            "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
            "  <module-set-id>1</module-set-id>"
            "</modules-state>"
            "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
            "  <mount-point>"
            "    <module>sm</module>"
            "    <label>root</label>"
            "    <inline/>"
            "  </mount-point>"
            "</schema-mounts>");
    xml =
            "<root xmlns=\"urn:sm\">\n"
            "  <interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>bu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "    </interface>\n"
            "  </interfaces>\n"
            "  <interfaces-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>bu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "      <oper-status>not-present</oper-status>\n"
            "      <statistics>\n"
            "        <discontinuity-time>2022-01-01T10:00:00-00:00</discontinuity-time>\n"
            "      </statistics>\n"
            "    </interface>\n"
            "  </interfaces-state>\n"
            "</root>\n";
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_SUCCESS, data);
    CHECK_LYD_STRING_PARAM(data, xml, LYD_XML, LYD_PRINT_WITHSIBLINGS);
    lyd_free_siblings(data);

    /* different yang-lib data */
    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb,
            "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\" "
            "    xmlns:ds=\"urn:ietf:params:xml:ns:yang:ietf-datastores\">"
            "  <module-set>"
            "    <name>test-set</name>"
            "    <module>"
            "      <name>ietf-datastores</name>"
            "      <revision>2018-02-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-datastores</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-library</name>"
            "      <revision>2019-01-04</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-library</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-schema-mount</name>"
            "      <revision>2019-01-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-interfaces</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-interfaces</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-ip</name>"
            "      <revision>2014-06-16</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-interfaces</namespace>"
            "    </module>"
            "    <module>"
            "      <name>iana-if-type</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:iana-if-type</namespace>"
            "    </module>"
            "    <import-only-module>"
            "      <name>ietf-yang-types</name>"
            "      <revision>2013-07-15</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-types</namespace>"
            "    </import-only-module>"
            "  </module-set>"
            "  <schema>"
            "    <name>test-schema</name>"
            "    <module-set>test-set</module-set>"
            "  </schema>"
            "  <datastore>"
            "    <name>ds:running</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <datastore>"
            "    <name>ds:operational</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <content-id>1</content-id>"
            "</yang-library>"
            "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
            "  <module-set-id>1</module-set-id>"
            "</modules-state>"
            "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
            "  <mount-point>"
            "    <module>sm</module>"
            "    <label>root</label>"
            "    <inline/>"
            "  </mount-point>"
            "</schema-mounts>");
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_SUCCESS, data);
    CHECK_LYD_STRING_PARAM(data, xml, LYD_XML, LYD_PRINT_WITHSIBLINGS);
    lyd_free_siblings(data);
}

static void
test_parse_xml_shared(void **state)
{
    const char *xml;
    struct lyd_node *data;

    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb,
            "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\" "
            "    xmlns:ds=\"urn:ietf:params:xml:ns:yang:ietf-datastores\">"
            "  <module-set>"
            "    <name>test-set</name>"
            "    <module>"
            "      <name>ietf-datastores</name>"
            "      <revision>2018-02-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-datastores</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-library</name>"
            "      <revision>2019-01-04</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-library</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-schema-mount</name>"
            "      <revision>2019-01-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-interfaces</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-interfaces</namespace>"
            "    </module>"
            "    <module>"
            "      <name>iana-if-type</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:iana-if-type</namespace>"
            "    </module>"
            "    <import-only-module>"
            "      <name>ietf-yang-types</name>"
            "      <revision>2013-07-15</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-types</namespace>"
            "    </import-only-module>"
            "  </module-set>"
            "  <schema>"
            "    <name>test-schema</name>"
            "    <module-set>test-set</module-set>"
            "  </schema>"
            "  <datastore>"
            "    <name>ds:running</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <datastore>"
            "    <name>ds:operational</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <content-id>1</content-id>"
            "</yang-library>"
            "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
            "  <module-set-id>1</module-set-id>"
            "</modules-state>"
            "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
            "  <mount-point>"
            "    <module>sm</module>"
            "    <label>root</label>"
            "    <shared-schema/>"
            "  </mount-point>"
            "</schema-mounts>");
    xml =
            "<root xmlns=\"urn:sm\">\n"
            "  <interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>bu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "    </interface>\n"
            "  </interfaces>\n"
            "  <interfaces-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>bu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "      <oper-status>not-present</oper-status>\n"
            "      <statistics>\n"
            "        <discontinuity-time>2022-01-01T10:00:00-00:00</discontinuity-time>\n"
            "      </statistics>\n"
            "    </interface>\n"
            "  </interfaces-state>\n"
            "</root>\n";
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_SUCCESS, data);
    CHECK_LYD_STRING_PARAM(data, xml, LYD_XML, LYD_PRINT_WITHSIBLINGS);
    lyd_free_siblings(data);

    /* different yang-lib data */
    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb,
            "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\" "
            "    xmlns:ds=\"urn:ietf:params:xml:ns:yang:ietf-datastores\">"
            "  <module-set>"
            "    <name>test-set</name>"
            "    <module>"
            "      <name>ietf-datastores</name>"
            "      <revision>2018-02-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-datastores</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-library</name>"
            "      <revision>2019-01-04</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-library</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-schema-mount</name>"
            "      <revision>2019-01-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-interfaces</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-interfaces</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-ip</name>"
            "      <revision>2014-06-16</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-interfaces</namespace>"
            "    </module>"
            "    <module>"
            "      <name>iana-if-type</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:iana-if-type</namespace>"
            "    </module>"
            "    <import-only-module>"
            "      <name>ietf-yang-types</name>"
            "      <revision>2013-07-15</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-types</namespace>"
            "    </import-only-module>"
            "  </module-set>"
            "  <schema>"
            "    <name>test-schema</name>"
            "    <module-set>test-set</module-set>"
            "  </schema>"
            "  <datastore>"
            "    <name>ds:running</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <datastore>"
            "    <name>ds:operational</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <content-id>2</content-id>"
            "</yang-library>"
            "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
            "  <module-set-id>1</module-set-id>"
            "</modules-state>"
            "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
            "  <mount-point>"
            "    <module>sm</module>"
            "    <label>root</label>"
            "    <shared-schema/>"
            "  </mount-point>"
            "</schema-mounts>");
    xml =
            "<root2 xmlns=\"urn:sm\">\n"
            "  <interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>bu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "    </interface>\n"
            "  </interfaces>\n"
            "  <interfaces-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>bu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "      <oper-status>not-present</oper-status>\n"
            "      <statistics>\n"
            "        <discontinuity-time>2022-01-01T10:00:00-00:00</discontinuity-time>\n"
            "      </statistics>\n"
            "    </interface>\n"
            "  </interfaces-state>\n"
            "</root2>\n";
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_EVALID, data);
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - Schema Mount, version 1\": "
            "Shared-schema yang-library content-id \"2\" differs from \"1\" used previously.",
            "/ietf-yang-library:yang-library/content-id");

    /* data for 2 mount points */
    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb,
            "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\" "
            "    xmlns:ds=\"urn:ietf:params:xml:ns:yang:ietf-datastores\">"
            "  <module-set>"
            "    <name>test-set</name>"
            "    <module>"
            "      <name>ietf-datastores</name>"
            "      <revision>2018-02-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-datastores</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-library</name>"
            "      <revision>2019-01-04</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-library</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-schema-mount</name>"
            "      <revision>2019-01-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-interfaces</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-interfaces</namespace>"
            "    </module>"
            "    <module>"
            "      <name>iana-if-type</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:iana-if-type</namespace>"
            "    </module>"
            "    <import-only-module>"
            "      <name>ietf-yang-types</name>"
            "      <revision>2013-07-15</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-types</namespace>"
            "    </import-only-module>"
            "  </module-set>"
            "  <schema>"
            "    <name>test-schema</name>"
            "    <module-set>test-set</module-set>"
            "  </schema>"
            "  <datastore>"
            "    <name>ds:running</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <datastore>"
            "    <name>ds:operational</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <content-id>1</content-id>"
            "</yang-library>"
            "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
            "  <module-set-id>1</module-set-id>"
            "</modules-state>"
            "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
            "  <mount-point>"
            "    <module>sm</module>"
            "    <label>root</label>"
            "    <shared-schema/>"
            "  </mount-point>"
            "</schema-mounts>");
    xml =
            "<root xmlns=\"urn:sm\">\n"
            "  <interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>bu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "    </interface>\n"
            "  </interfaces>\n"
            "  <interfaces-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>bu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "      <oper-status>not-present</oper-status>\n"
            "      <statistics>\n"
            "        <discontinuity-time>2022-01-01T10:00:00-00:00</discontinuity-time>\n"
            "      </statistics>\n"
            "    </interface>\n"
            "  </interfaces-state>\n"
            "</root>\n"
            "<root2 xmlns=\"urn:sm\">\n"
            "  <interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>fu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:fddi</type>\n"
            "    </interface>\n"
            "  </interfaces>\n"
            "  <interfaces-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>fu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:fddi</type>\n"
            "      <oper-status>down</oper-status>\n"
            "      <statistics>\n"
            "        <discontinuity-time>2020-01-01T10:00:00-00:00</discontinuity-time>\n"
            "      </statistics>\n"
            "    </interface>\n"
            "  </interfaces-state>\n"
            "</root2>\n";
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_SUCCESS, data);
    CHECK_LYD_STRING_PARAM(data, xml, LYD_XML, LYD_PRINT_WITHSIBLINGS);
    lyd_free_siblings(data);
}

static void
test_parse_xml_shared_parent_ref(void **state)
{
    const char *xml;
    struct lyd_node *data;

    /* wrong leafref value */
    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb,
            "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\" "
            "    xmlns:ds=\"urn:ietf:params:xml:ns:yang:ietf-datastores\">"
            "  <module-set>"
            "    <name>test-set</name>"
            "    <module>"
            "      <name>ietf-datastores</name>"
            "      <revision>2018-02-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-datastores</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-library</name>"
            "      <revision>2019-01-04</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-library</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-schema-mount</name>"
            "      <revision>2019-01-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount</namespace>"
            "    </module>"
            "    <module>"
            "      <name>sm</name>"
            "      <namespace>urn:sm</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-interfaces</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-interfaces</namespace>"
            "    </module>"
            "    <module>"
            "      <name>iana-if-type</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:iana-if-type</namespace>"
            "    </module>"
            "    <import-only-module>"
            "      <name>ietf-yang-types</name>"
            "      <revision>2013-07-15</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-types</namespace>"
            "    </import-only-module>"
            "  </module-set>"
            "  <schema>"
            "    <name>test-schema</name>"
            "    <module-set>test-set</module-set>"
            "  </schema>"
            "  <datastore>"
            "    <name>ds:running</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <datastore>"
            "    <name>ds:operational</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <content-id>1</content-id>"
            "</yang-library>"
            "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
            "  <module-set-id>1</module-set-id>"
            "</modules-state>"
            "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
            "  <namespace>"
            "    <prefix>smp</prefix>"
            "    <uri>urn:sm</uri>"
            "  </namespace>"
            "  <mount-point>"
            "    <module>sm</module>"
            "    <label>mnt-root</label>"
            "    <shared-schema>"
            "      <parent-reference>/smp:target[. = current()/../smp:name]</parent-reference>"
            "    </shared-schema>"
            "  </mount-point>"
            "</schema-mounts>");
    xml =
            "<root3 xmlns=\"urn:sm\">\n"
            "  <ls>\n"
            "    <name>target-value</name>\n"
            "    <mnt-root>\n"
            "      <interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "        <interface>\n"
            "          <name>bu</name>\n"
            "          <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "          <sm-name xmlns=\"urn:sm\">target-value</sm-name>\n"
            "        </interface>\n"
            "      </interfaces>\n"
            "    </mnt-root>\n"
            "  </ls>\n"
            "</root3>\n"
            "<target xmlns=\"urn:sm\">wrong-target-value</target>\n";
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_EVALID, data);
    CHECK_LOG_CTX("Extension plugin \"libyang 2 - Schema Mount, version 1\": "
            "Invalid leafref value \"target-value\" - no existing target instance \"/sm:target\".",
            "Schema location /ietf-interfaces:interfaces/interface/sm:sm-name, "
            "data location /ietf-interfaces:interfaces/interface[name='bu']/sm:sm-name.");

    /* success */
    xml =
            "<root3 xmlns=\"urn:sm\">\n"
            "  <ls>\n"
            "    <name>target-value</name>\n"
            "    <mnt-root>\n"
            "      <interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "        <interface>\n"
            "          <name>bu</name>\n"
            "          <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "          <sm-name xmlns=\"urn:sm\">target-value</sm-name>\n"
            "        </interface>\n"
            "      </interfaces>\n"
            "    </mnt-root>\n"
            "  </ls>\n"
            "</root3>\n"
            "<target xmlns=\"urn:sm\">target-value</target>\n";
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_SUCCESS, data);
    CHECK_LYD_STRING_PARAM(data, xml, LYD_XML, LYD_PRINT_WITHSIBLINGS);
    lyd_free_siblings(data);
}

static void
test_parse_xml_config(void **state)
{
    const char *xml;
    struct lyd_node *data;
    const struct lyd_node *node;

    ly_ctx_set_ext_data_clb(UTEST_LYCTX, test_ext_data_clb,
            "<yang-library xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\" "
            "    xmlns:ds=\"urn:ietf:params:xml:ns:yang:ietf-datastores\">"
            "  <module-set>"
            "    <name>test-set</name>"
            "    <module>"
            "      <name>ietf-datastores</name>"
            "      <revision>2018-02-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-datastores</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-library</name>"
            "      <revision>2019-01-04</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-library</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-yang-schema-mount</name>"
            "      <revision>2019-01-14</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount</namespace>"
            "    </module>"
            "    <module>"
            "      <name>ietf-interfaces</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-interfaces</namespace>"
            "    </module>"
            "    <module>"
            "      <name>iana-if-type</name>"
            "      <revision>2014-05-08</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:iana-if-type</namespace>"
            "    </module>"
            "    <import-only-module>"
            "      <name>ietf-yang-types</name>"
            "      <revision>2013-07-15</revision>"
            "      <namespace>urn:ietf:params:xml:ns:yang:ietf-yang-types</namespace>"
            "    </import-only-module>"
            "  </module-set>"
            "  <schema>"
            "    <name>test-schema</name>"
            "    <module-set>test-set</module-set>"
            "  </schema>"
            "  <datastore>"
            "    <name>ds:running</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <datastore>"
            "    <name>ds:operational</name>"
            "    <schema>test-schema</schema>"
            "  </datastore>"
            "  <content-id>1</content-id>"
            "</yang-library>"
            "<modules-state xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-library\">"
            "  <module-set-id>1</module-set-id>"
            "</modules-state>"
            "<schema-mounts xmlns=\"urn:ietf:params:xml:ns:yang:ietf-yang-schema-mount\">"
            "  <mount-point>"
            "    <module>sm</module>"
            "    <label>root</label>"
            "    <config>false</config>"
            "    <inline/>"
            "  </mount-point>"
            "</schema-mounts>");
    xml =
            "<root xmlns=\"urn:sm\">\n"
            "  <interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">\n"
            "    <interface>\n"
            "      <name>bu</name>\n"
            "      <type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>\n"
            "      <enabled>true</enabled>\n"
            "    </interface>\n"
            "  </interfaces>\n"
            "</root>\n";
    CHECK_PARSE_LYD_PARAM(xml, LYD_XML, LYD_PARSE_STRICT, LYD_VALIDATE_PRESENT, LY_SUCCESS, data);
    CHECK_LYD_STRING_PARAM(data, xml, LYD_XML, LYD_PRINT_WITHSIBLINGS);

    node = lyd_child(data);
    assert_string_equal(LYD_NAME(node), "interfaces");
    assert_true(node->schema->flags & LYS_CONFIG_R);
    node = lyd_child(node);
    assert_string_equal(LYD_NAME(node), "interface");
    assert_true(node->schema->flags & LYS_CONFIG_R);
    node = lyd_child(node);
    assert_string_equal(LYD_NAME(node), "name");
    assert_true(node->schema->flags & LYS_CONFIG_R);
    node = node->next;
    assert_string_equal(LYD_NAME(node), "type");
    assert_true(node->schema->flags & LYS_CONFIG_R);

    lyd_free_siblings(data);
}

int
main(void)
{
    const struct CMUnitTest tests[] = {
        UTEST(test_schema),
        UTEST(test_parse_xml_invalid, setup),
        UTEST(test_parse_xml_inline, setup),
        UTEST(test_parse_xml_shared, setup),
        UTEST(test_parse_xml_shared_parent_ref, setup),
        UTEST(test_parse_xml_config, setup),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}