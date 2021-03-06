# This script was automatically generated from 
#  http://www.gentoo.org/security/en/glsa/glsa-200502-10.xml
# It is released under the Nessus Script Licence.
# The messages are release under the Creative Commons - Attribution /
# Share Alike license. See http://creativecommons.org/licenses/by-sa/2.0/
#
# Avisory is copyright 2001-2005 Gentoo Foundation, Inc.
# GLSA2nasl Convertor is copyright 2004 Michel Arboi <mikhail@nessus.org>

if (! defined_func('bn_random')) exit(0);

if (description)
{
 script_id(16447);
 script_version("$Revision: 1.3 $");
 script_xref(name: "GLSA", value: "200502-10");
 script_cve_id("CVE-2005-0064");

 desc = 'The remote host is affected by the vulnerability described in GLSA-200502-10
(pdftohtml: Vulnerabilities in included Xpdf)


    Xpdf is vulnerable to a buffer overflow, as described in GLSA
    200501-28.
  
Impact

    An attacker could entice a user to convert a specially-crafted PDF
    file, potentially resulting in the execution of arbitrary code with the
    rights of the user running pdftohtml.
  
Workaround

    There is no known workaround at this time.
  
References:
    http://www.gentoo.org/security/en/glsa/glsa-200501-28.xml
    http://cve.mitre.org/cgi-bin/cvename.cgi?name=CVE-2005-0064


Solution: 
    All pdftohtml users should upgrade to the latest version:
    # emerge --sync
    # emerge --ask --oneshot --verbose ">=app-text/pdftohtml-0.36-r3"
  

Risk factor : Medium
';
 script_description(english: desc);
 script_copyright(english: "(C) 2005 Michel Arboi <mikhail@nessus.org>");
 script_name(english: "[GLSA-200502-10] pdftohtml: Vulnerabilities in included Xpdf");
 script_category(ACT_GATHER_INFO);
 script_family(english: "Gentoo Local Security Checks");
 script_dependencies("ssh_get_info.nasl");
 script_require_keys('Host/Gentoo/qpkg-list');
 script_summary(english: 'pdftohtml: Vulnerabilities in included Xpdf');
 exit(0);
}

include('qpkg.inc');
if (qpkg_check(package: "app-text/pdftohtml", unaffected: make_list("ge 0.36-r3"), vulnerable: make_list("lt 0.36-r3")
)) { security_warning(0); exit(0); }
