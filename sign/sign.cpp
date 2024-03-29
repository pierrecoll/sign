//-------------------------------------------------------------------
// Example C Program: 
// Signs a message by using a sender's private key and encrypts the
// signed message by using a receiver's public key.
//https://docs.microsoft.com/fr-fr/windows/desktop/SecCrypto/example-c-program-sending-and-receiving-a-signed-and-encrypted-message

#include "pch.h"
//#include <iostream>
#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <Wincrypt.h>
#include <cryptuiapi.h>

#pragma warning(disable : 4996)
#pragma comment(lib, "crypt32.lib")
#pragma comment (lib, "cryptui.lib")


#define MY_ENCODING_TYPE (PKCS_7_ASN_ENCODING | X509_ASN_ENCODING)
#define MAX_NAME  128

//-------------------------------------------------------------------
//  This example uses the function MyHandleError, a simple error
//  handling function, to print an error message to the standard  
//  error (stderr) file and exit the program. 
//  For most applications, replace this function with one 
//  that does more extensive error reporting.

void MyHandleError(PTSTR psz)
{
	_tprintf(L"An error occurred in the program. \n");
	_tprintf(L"%s\n", psz);
	_tprintf(L"Error number %x.\n", GetLastError());
	//_tprintf(L"Program terminating. \n");
	//exit(1);
} // End of MyHandleError.

//https://support.microsoft.com/en-us/help/138813/how-to-convert-from-ansi-to-unicode-unicode-to-ansi-for-ole
/*
 * AnsiToUnicode converts the ANSI string pszA to a Unicode string
 * and returns the Unicode string through ppszW. Space for the
 * the converted string is allocated by AnsiToUnicode.
 */

HRESULT __fastcall AnsiToUnicode(LPCSTR pszA, LPOLESTR* ppszW)
{

	ULONG cCharacters;
	DWORD dwError;

	// If input is null then just return the same.
	if (NULL == pszA)
	{
		*ppszW = NULL;
		return NOERROR;
	}

	// Determine number of wide characters to be allocated for the
	// Unicode string.
	cCharacters = strlen(pszA) + 1;

	// Use of the OLE allocator is required if the resultant Unicode
	// string will be passed to another COM component and if that
	// component will free it. Otherwise you can use your own allocator.
	*ppszW = (LPOLESTR)CoTaskMemAlloc(cCharacters * 2);
	if (NULL == *ppszW)
		return E_OUTOFMEMORY;

	// Covert to Unicode.
	if (0 == MultiByteToWideChar(CP_ACP, 0, pszA, cCharacters,
		*ppszW, cCharacters))
	{
		dwError = GetLastError();
		CoTaskMemFree(*ppszW);
		*ppszW = NULL;
		return HRESULT_FROM_WIN32(dwError);
	}

	return NOERROR;
}
//-------------------------------------------------------------------
// The local function ShowBytes is declared here and defined after 
// main.

void ShowBytes(BYTE *s, DWORD len);

//-------------------------------------------------------------------
// Declare local functions SignAndEncrypt, DecryptAndVerify, and 
// WriteSignedAndEncryptedBlob.
// These functions are defined after main.

BYTE* SignAndEncrypt(WCHAR *SignerName,
	const BYTE     *pbToBeSignedAndEncrypted,
	DWORD          cbToBeSignedAndEncrypted,
	DWORD          *pcbSignedAndEncryptedBlob);

BYTE* DecryptAndVerify(
	BYTE  *pbSignedAndEncryptedBlob,
	DWORD  cbSignedAndEncryptedBlob);

void WriteSignedAndEncryptedBlob(
	DWORD  cbBlob,
	BYTE   *pbBlob);

int main(int argc, _TCHAR* argv[])
{
	//---------------------------------------------------------------
	// Declare and initialize local variables.

	//---------------------------------------------------------------
	//  pbToBeSignedAndEncrypted is the message to be 
	//  encrypted and signed.

	const BYTE *pbToBeSignedAndEncrypted =
		(const unsigned char *)"This is the message to be encrypted!";
	//---------------------------------------------------------------
	// This is the length of the message to be
	// encrypted and signed. Note that it is one
	// more that the length returned by strlen()
	// to include the terminating null character.

	DWORD cbToBeSignedAndEncrypted =
		lstrlenA((const char *)pbToBeSignedAndEncrypted) + 1;

	//---------------------------------------------------------------
	// Pointer to a buffer that will hold the
	// encrypted and signed message.

	BYTE *pbSignedAndEncryptedBlob;

	//---------------------------------------------------------------
	// A DWORD to hold the length of the signed 
	// and encrypted message.

	DWORD cbSignedAndEncryptedBlob;
	BYTE *pReturnMessage;

	if (argc < 2)
	{
		printf("Missing signer name parameter\r\n");
		printf("Usage: sign SignerName\r\n");
		exit(-1L);
	}
	WCHAR *SignerName[256];
	AnsiToUnicode((LPCSTR)argv[1], (LPOLESTR*)SignerName);
	wprintf(L"Using signer name:%s\r\n",*SignerName);
	wprintf(L"Message to be encrypted:%S\r\n", pbToBeSignedAndEncrypted);
	//---------------------------------------------------------------
	// Call the local function SignAndEncrypt.
	// This function returns a pointer to the 
	// signed and encrypted BLOB and also returns
	// the length of that BLOB.

	pbSignedAndEncryptedBlob = SignAndEncrypt(
		(WCHAR*)*SignerName,
		pbToBeSignedAndEncrypted,
		cbToBeSignedAndEncrypted,
		&cbSignedAndEncryptedBlob);

	wprintf(L"The following is the signed and encrypted message.\n");
	ShowBytes(pbSignedAndEncryptedBlob, cbSignedAndEncryptedBlob / 4);

	// Open a file and write the signed and 
	// encrypted message to the file.

	WriteSignedAndEncryptedBlob(
		cbSignedAndEncryptedBlob,
		pbSignedAndEncryptedBlob);

	//---------------------------------------------------------------
	// Call the local function DecryptAndVerify.
	// This function decrypts and displays the 
	// encrypted message and also verifies the 
	// message's signature.

	if (pReturnMessage = DecryptAndVerify(
		pbSignedAndEncryptedBlob,
		cbSignedAndEncryptedBlob))
	{
		_tprintf(L"The returned, verified message is ->\n%S\n",(char *)pReturnMessage);
		_tprintf(L"The program executed without error.\n");
	}
	else
	{
		_tprintf(L"Verification failed.\n");
	}

} // End Main.

//-------------------------------------------------------------------
// Begin definition of the SignAndEncrypt function.

BYTE* SignAndEncrypt(
	WCHAR *SignerName,
	const BYTE *pbToBeSignedAndEncrypted,
	DWORD cbToBeSignedAndEncrypted,
	DWORD *pcbSignedAndEncryptedBlob)
{
	//---------------------------------------------------------------
	// Declare and initialize local variables.

	//FILE *hToSave;
	HCERTSTORE hCertStore;

	//---------------------------------------------------------------
	// pSignerCertContext will be the certificate of 
	// the message signer.

	PCCERT_CONTEXT pSignerCertContext;

	//---------------------------------------------------------------
	// pReceiverCertContext will be the certificate of the 
	// message receiver.

	PCCERT_CONTEXT pReceiverCertContext;

	TCHAR pszNameString[256];
	CRYPT_SIGN_MESSAGE_PARA SignPara;
	CRYPT_ENCRYPT_MESSAGE_PARA EncryptPara;
	DWORD cRecipientCert;
	PCCERT_CONTEXT rgpRecipientCert[5];
	BYTE *pbSignedAndEncryptedBlob = NULL;
	CERT_NAME_BLOB Subject_Blob;
	//BYTE *pbDataIn;
	DWORD dwKeySpec;
	HCRYPTPROV hCryptProv;

	//---------------------------------------------------------------
	// Open the MY certificate store. 
	// For more information, see the CertOpenStore function 
	// PSDK reference page. 
	// Note: Case is not significant in certificate store names.
	wprintf(L"Calling CertOpenStore for my store\n");
	if (!(hCertStore = CertOpenStore(
		CERT_STORE_PROV_SYSTEM,
		0,
		NULL,
		CERT_SYSTEM_STORE_CURRENT_USER,
		L"my")))
	{
		MyHandleError((LPTSTR)L"The MY store could not be opened.");
		exit(1L);
	}

	//---------------------------------------------------------------
	// Get the certificate for the signer.
	wprintf(L"Calling CryptUIDlgSelectCertificateFromStore\n");
	if (!(pSignerCertContext = CryptUIDlgSelectCertificateFromStore(
		hCertStore,
		NULL,
		NULL,
		NULL,
		0,
		0,
		0)))
	{
		MyHandleError((LPTSTR)L"No certificate selected.\n");
	}
	else
	{
		goto certselected;
	}

	wprintf(L"Calling CertFindCertificateInStore with signer's name %s \n", SignerName);

	if (!(pSignerCertContext = CertFindCertificateInStore(
		hCertStore,
		MY_ENCODING_TYPE,
		0,
		CERT_FIND_SUBJECT_STR,
		(void*)SignerName,
		NULL)))
	{
		MyHandleError((LPTSTR)L"Cert not found.\n");
		exit(1L);
	}

certselected:

	//---------------------------------------------------------------
	// Get and print the name of the message signer.
	// The following two calls to CertGetNameString with different
	// values for the second parameter get two different forms of 
	// the certificate subject's name.

	if (CertGetNameString(
		pSignerCertContext,
		CERT_NAME_SIMPLE_DISPLAY_TYPE,
		0,
		NULL,
		pszNameString,
		MAX_NAME) > 1)
	{
		wprintf(L"The SIMPLE_DISPLAY_TYPE message signer's name is %s \n", pszNameString);
	}
	else
	{
		MyHandleError((LPTSTR)L"Getting the name of the signer failed.\n");
		exit(1L);
	}

	if (CertGetNameString(
		pSignerCertContext,
		CERT_NAME_RDN_TYPE,
		0,
		NULL,
		pszNameString,
		MAX_NAME) > 1)
	{
		_tprintf(L"The RDM_TYPE message signer's name is %s \n", pszNameString);
	}
	else
	{
		MyHandleError((LPTSTR)L"Getting the name of the signer failed.\n");
	}

	// if display asked
	if (CryptUIDlgViewContext(
		CERT_STORE_CERTIFICATE_CONTEXT,
		pSignerCertContext,
		NULL,
		NULL,
		0,
		NULL))
	{
		//     printf("OK\n");
	}
	else
	{
		MyHandleError((LPTSTR)L"UI failed.");
	}

	wprintf(L"Calling CryptAcquireCertificatePrivateKey\r\n");
	if (!(CryptAcquireCertificatePrivateKey(
		pSignerCertContext,
		0,
		NULL,
		&hCryptProv,
		&dwKeySpec,
		NULL)))
	{
		MyHandleError((LPTSTR)L"CryptAcquireCertificatePrivateKey.\n");	
		if (GetLastError() == NTE_BAD_PROV_TYPE)
		{
			goto allow;
		}
		exit(-1L);
	}
	wprintf(L" CryptAcquireCertificatePrivateKey succeeded\r\n");
	goto acquire;

allow:
	wprintf(L"Calling CryptAcquireCertificatePrivateKey with CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG\r\n");
	if (!(CryptAcquireCertificatePrivateKey(
		pSignerCertContext,
		CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG,
		NULL,
		&hCryptProv,
		&dwKeySpec,
		NULL)))
	{
		MyHandleError((LPTSTR)L"CryptAcquireCertificatePrivateKey.\n");		
		if (GetLastError() == NTE_BAD_PROV_TYPE)
		{
			goto prefer;
		}
		exit(-1L);
	}
	wprintf(L" CryptAcquireCertificatePrivateKey with CRYPT_ACQUIRE_ALLOW_NCRYPT_KEY_FLAG succeeded\r\n");
	goto acquire;

prefer:	
		wprintf(L"Calling CryptAcquireCertificatePrivateKey with CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG\r\n");
	if (!(CryptAcquireCertificatePrivateKey(
		pSignerCertContext,
		CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG,
		NULL,
		&hCryptProv,
		&dwKeySpec,
		NULL)))
	{
		MyHandleError((LPTSTR)L"CryptAcquireCertificatePrivateKey.\n");
		goto acquire;
	}
	wprintf(L" CryptAcquireCertificatePrivateKey with CRYPT_ACQUIRE_PREFER_NCRYPT_KEY_FLAG succeeded\r\n");
acquire:
	//---------------------------------------------------------------
// Declare and initialize variables.
	//HCRYPTPROV hCryptProv;

	//---------------------------------------------------------------

	if (dwKeySpec != CERT_NCRYPT_KEY_SPEC)
	{
		//This API is deprecated.New and existing software should start using Cryptography Next Generation APIs.Microsoft may remove this API in future releases.
		// Get a handle to the default PROV_RSA_FULL provider.
		if (CryptAcquireContext(
			&hCryptProv,
			NULL,
			NULL,
			PROV_RSA_FULL,
			0))
		{
			_tprintf(L"CryptAcquireContext succeeded.\n");
		}
		else
		{
			if (GetLastError() == NTE_BAD_KEYSET)
			{
				// No default container was found. Attempt to create it.
				if (CryptAcquireContext(
					&hCryptProv,
					NULL,
					NULL,
					PROV_RSA_FULL,
					CRYPT_NEWKEYSET))
				{
					_tprintf(L"CryptAcquireContext succeeded.\n");
				}
				else
				{
					MyHandleError((LPTSTR)L"Could not create the default key container.\n");
				}
			}
			else
			{
				MyHandleError((LPTSTR)L"A general error running CryptAcquireContext.");
			}
		}

		CHAR pszName[1000];
		DWORD cbName;

		//---------------------------------------------------------------
		// Read the name of the CSP.
		cbName = 1000;
		if (CryptGetProvParam(
			hCryptProv,
			PP_NAME,
			(BYTE*)pszName,
			&cbName,
			0))
		{
			_tprintf(L"CryptGetProvParam succeeded.\n");
			printf("Provider name: %s\n", pszName);
		}
		else
		{
			MyHandleError((LPTSTR)L"Error reading CSP name.\n");
		}

		//---------------------------------------------------------------
		// Read the name of the key container.
		cbName = 1000;
		if (CryptGetProvParam(
			hCryptProv,
			PP_CONTAINER,
			(BYTE*)pszName,
			&cbName,
			0))
		{
			_tprintf(L"CryptGetProvParam succeeded.\n");
			printf("Key Container name: %s\n", pszName);
		}
		else
		{
			MyHandleError((LPTSTR)L"Error reading key container name.\n");
		}
	} 
	else
	{
		NCRYPT_KEY_HANDLE KeyHandle = hCryptProv;
		printf("CryptAcquireCertificatePrivateKey returned a CNG Key handle: %X\n", KeyHandle);

		//NCryptGetProperty(KeyHandle, );
	}
	
	//---------------------------------------------------------------
	// Perform cryptographic operations.
	//...



	//---------------------------------------------------------------
	// Get the certificate for the receiver. In this case,  
	// a BLOB with the name of the receiver is saved in a file.

	// Note: To decrypt the message signed and encrypted here,
	// this program must use the certificate of the intended
	// receiver. The signed and encrypted message can only be
	// decrypted and verified by the owner of the recipient
	// certificate. That user must have access to the private key
	// associated with the public key of the recipient's certificate.

	// To run this sample, the file contains information that allows 
	// the program to find one of the current user's certificates. 
	// The current user should have access to the private key of the
	// certificate and thus can test the verification and decryption. 

	// In normal use, the file would contain information used to find
	// the certificate of an intended receiver of the message. 
	// The signed and encrypted message would be written
	// to a file or otherwise sent to the intended receiver.

	//---------------------------------------------------------------
	// Open a file and read in the receiver name
	// BLOB.


	/*if (!(hToSave = fopen("s.txt", "rb")))
	{
		MyHandleError((LPTSTR)L"Source file was not opened.\n");
	}

	fread(
		&(Subject_Blob.cbData),
		sizeof(DWORD),
		1,
		hToSave);

	if (ferror(hToSave))
	{
		MyHandleError((LPTSTR)L"The size of the BLOB was not read.\n");
	}

	if (!(pbDataIn = (BYTE *)malloc(Subject_Blob.cbData)))
	{
		MyHandleError((LPTSTR)L"Memory allocation error.");
	}

	fread(
		pbDataIn,
		Subject_Blob.cbData,
		1,
		hToSave);

	if (ferror(hToSave))
	{
		MyHandleError((LPTSTR)L"BLOB not read.");
	}

	fclose(hToSave);*/

	Subject_Blob.cbData = lstrlenA((const char *)"Pierre-Louis Coll") + 1;
	Subject_Blob.pbData = (BYTE*)"Pierre-Louis Coll";

	//---------------------------------------------------------------
	// Use the BLOB just read in from the file to find its associated
	// certificate in the MY store.
	// This call to CertFindCertificateInStore uses the
	// CERT_FIND_SUBJECT_NAME dwFindType.

	if (!(pReceiverCertContext = CertFindCertificateInStore(
		hCertStore,
		MY_ENCODING_TYPE,
		0,
		//CERT_FIND_SUBJECT_NAME,
		CERT_FIND_ANY,
		&Subject_Blob,
		NULL)))
	{
		MyHandleError((LPTSTR)L"Receiver certificate not found.");
	}

	//---------------------------------------------------------------
	// Get and print the subject name from the receiver's
	// certificate.

	if (CertGetNameString(
		pReceiverCertContext,
		CERT_NAME_SIMPLE_DISPLAY_TYPE,
		0,
		NULL,
		pszNameString,
		MAX_NAME) > 1)
	{
		_tprintf(L"The message receiver is  %s \n",
			pszNameString);
	}
	else
	{
		MyHandleError(
			(LPTSTR)"Getting the name of the receiver failed.\n");
	}

	//---------------------------------------------------------------
	// Initialize variables and data structures
	// for the call to CryptSignAndEncryptMessage.

	SignPara.cbSize = sizeof(CRYPT_SIGN_MESSAGE_PARA);
	SignPara.dwMsgEncodingType = MY_ENCODING_TYPE;
	SignPara.pSigningCert = pSignerCertContext;
	SignPara.HashAlgorithm.pszObjId = (LPSTR)szOID_RSA_MD2;
	SignPara.HashAlgorithm.Parameters.cbData = 0;
	SignPara.pvHashAuxInfo = NULL;
	SignPara.cMsgCert = 1;
	SignPara.rgpMsgCert = &pSignerCertContext;
	SignPara.cMsgCrl = 0;
	SignPara.rgpMsgCrl = NULL;
	SignPara.cAuthAttr = 0;
	SignPara.rgAuthAttr = NULL;
	SignPara.cUnauthAttr = 0;
	SignPara.rgUnauthAttr = NULL;
	SignPara.dwFlags = 0;
	SignPara.dwInnerContentType = 0;

	EncryptPara.cbSize = sizeof(CRYPT_ENCRYPT_MESSAGE_PARA);
	EncryptPara.dwMsgEncodingType = MY_ENCODING_TYPE;
	EncryptPara.hCryptProv = 0;
	EncryptPara.ContentEncryptionAlgorithm.pszObjId = (LPSTR)szOID_RSA_RC4;
	EncryptPara.ContentEncryptionAlgorithm.Parameters.cbData = 0;
	EncryptPara.pvEncryptionAuxInfo = NULL;
	EncryptPara.dwFlags = 0;
	EncryptPara.dwInnerContentType = 0;

	cRecipientCert = 1;
	rgpRecipientCert[0] = pReceiverCertContext;
	*pcbSignedAndEncryptedBlob = 0;
	pbSignedAndEncryptedBlob = NULL;

	if (CryptSignAndEncryptMessage(
		&SignPara,
		&EncryptPara,
		cRecipientCert,
		rgpRecipientCert,
		pbToBeSignedAndEncrypted,
		cbToBeSignedAndEncrypted,
		NULL, // the pbSignedAndEncryptedBlob
		pcbSignedAndEncryptedBlob))
	{
		_tprintf(L"%d bytes for the buffer .\n",
			*pcbSignedAndEncryptedBlob);
	}
	else
	{
		MyHandleError((LPTSTR)L"Getting the buffer length failed.");
	}

	//---------------------------------------------------------------
	// Allocate memory for the buffer.

	if (!(pbSignedAndEncryptedBlob =
		(unsigned char *)malloc(*pcbSignedAndEncryptedBlob)))
	{
		MyHandleError((LPTSTR)L"Memory allocation failed.");
	}

	//---------------------------------------------------------------
	// Call the function a second time to copy the signed and 
	// encrypted message into the buffer.

	if (CryptSignAndEncryptMessage(
		&SignPara,
		&EncryptPara,
		cRecipientCert,
		rgpRecipientCert,
		pbToBeSignedAndEncrypted,
		cbToBeSignedAndEncrypted,
		pbSignedAndEncryptedBlob,
		pcbSignedAndEncryptedBlob))
	{
		_tprintf(L"The message is signed and encrypted.\n");
	}
	else
	{
		MyHandleError(
			(LPTSTR)"The message failed to sign and encrypt.");
	}

	//---------------------------------------------------------------
	// Clean up.

	if (pSignerCertContext)
	{
		CertFreeCertificateContext(pSignerCertContext);
	}

	if (pReceiverCertContext)
	{
		CertFreeCertificateContext(pReceiverCertContext);
	}

	CertCloseStore(hCertStore, 0);

	//---------------------------------------------------------------
	// Return the signed and encrypted message.

	return pbSignedAndEncryptedBlob;

}  // End SignAndEncrypt.

//-------------------------------------------------------------------
// Define the DecryptAndVerify function.

BYTE* DecryptAndVerify(
	BYTE  *pbSignedAndEncryptedBlob,
	DWORD  cbSignedAndEncryptedBlob)
{
	//---------------------------------------------------------------
	// Declare and initialize local variables.

	HCERTSTORE hCertStore;
	CRYPT_DECRYPT_MESSAGE_PARA DecryptPara;
	CRYPT_VERIFY_MESSAGE_PARA VerifyPara;
	DWORD dwSignerIndex = 0;
	BYTE *pbDecrypted;
	DWORD cbDecrypted;

	//---------------------------------------------------------------
	// Open the certificate store.

	if (!(hCertStore = CertOpenStore(
		CERT_STORE_PROV_SYSTEM,
		0,
		NULL,
		CERT_SYSTEM_STORE_CURRENT_USER,
		L"my")))
	{
		MyHandleError((LPTSTR)L"The MY store could not be opened.");
	}

	//---------------------------------------------------------------
	// Initialize the needed data structures.

	DecryptPara.cbSize = sizeof(CRYPT_DECRYPT_MESSAGE_PARA);
	DecryptPara.dwMsgAndCertEncodingType = MY_ENCODING_TYPE;
	DecryptPara.cCertStore = 1;
	DecryptPara.rghCertStore = &hCertStore;

	VerifyPara.cbSize = sizeof(CRYPT_VERIFY_MESSAGE_PARA);
	VerifyPara.dwMsgAndCertEncodingType = MY_ENCODING_TYPE;
	VerifyPara.hCryptProv = 0;
	VerifyPara.pfnGetSignerCertificate = NULL;
	VerifyPara.pvGetArg = NULL;
	pbDecrypted = NULL;
	cbDecrypted = 0;

	//---------------------------------------------------------------
	// Call CryptDecryptAndVerifyMessageSignature a first time
	// to determine the needed size of the buffer to hold the 
	// decrypted message.

	if (!(CryptDecryptAndVerifyMessageSignature(
		&DecryptPara,
		&VerifyPara,
		dwSignerIndex,
		pbSignedAndEncryptedBlob,
		cbSignedAndEncryptedBlob,
		NULL,           // pbDecrypted
		&cbDecrypted,
		NULL,
		NULL)))
	{
		MyHandleError((LPTSTR)L"Failed getting size.");
	}

	//---------------------------------------------------------------
	// Allocate memory for the buffer to hold the decrypted
	// message.

	if (!(pbDecrypted = (BYTE *)malloc(cbDecrypted)))
	{
		MyHandleError((LPTSTR)L"Memory allocation failed.");
	}

	if (!(CryptDecryptAndVerifyMessageSignature(
		&DecryptPara,
		&VerifyPara,
		dwSignerIndex,
		pbSignedAndEncryptedBlob,
		cbSignedAndEncryptedBlob,
		pbDecrypted,
		&cbDecrypted,
		NULL,
		NULL)))
	{
		pbDecrypted = NULL;
	}

	//---------------------------------------------------------------
	// Close the certificate store.

	CertCloseStore(
		hCertStore,
		0);

	//---------------------------------------------------------------
	// Return the decrypted string or NULL.

	return pbDecrypted;

} // End of DecryptandVerify.

//-------------------------------------------------------------------
// Define the MyHandleError function.

void WriteSignedAndEncryptedBlob(
	DWORD cbBlob,
	BYTE *pbBlob)
{
	// Open an output file, write the file, and close the file.
	// This function would be used to save the signed and encrypted 
	// message to a file that would be sent to the intended receiver.
	// Note: The only receiver able to decrypt and verify this
	// message will have access to the private key associated 
	// with the public key from the certificate used when 
	// the message was encrypted.

	_tprintf(L"%d bytes for the buffer .\n",
		*pbBlob);

	FILE *hOutputFile;

	if (!(hOutputFile = _tfopen((LPTSTR)L"signedandencryptedblob.txt", (LPTSTR)"w")))
	{
		MyHandleError((LPTSTR)L"Error creating the file\n");
	}

	fwrite(
		&cbBlob,
		sizeof(DWORD),
		1,
		hOutputFile);

	if (ferror(hOutputFile))
	{
		MyHandleError(
			(LPTSTR)"The size of the BLOB was not written.\n");
	}

	fwrite(
		pbBlob,
		cbBlob,
		1,
		hOutputFile);

	if (ferror(hOutputFile))
	{
		MyHandleError(
			(LPTSTR)"The bytes of the BLOB were not written.\n");
	}
	else
	{
		_tprintf(L"The BLOB has been written to the file.\n");
	}

	fclose(hOutputFile);

}  // End of WriteSignedAndEcryptedBlob.


//-------------------------------------------------------------------
// Define the ShowBytes function.
// This function displays the contents of a BYTE buffer. Characters
// less than '0' or greater than 'z' are all displayed as '-'.

void ShowBytes(BYTE *s, DWORD len)
{
	DWORD TotalChars = 0;
	DWORD ThisLine = 0;

	while (TotalChars < len)
	{
		if (ThisLine > 70)
		{
			ThisLine = 0;
			_tprintf(L"\n");
		}
		if (s[TotalChars] < '0' || s[TotalChars] > 'z')
		{
			_tprintf(L"-");
		}
		else
		{
			_tprintf(L"%c", s[TotalChars]);
		}

		TotalChars++;
		ThisLine++;
	}

	_tprintf(L"\n");
} // End of ShowBytes.