{
    'targets' : [
	{
	    'target_name' : 'http-multi-sync',
	    'sources' : [
		'http-multi-sync.cc'
	    ],
	    'libraries' : [
		'-lcurl'
	    ],
	    'include_dirs': [
		"<!(node -e \"require('nan')\")"
	    ],
	}
    ]
}
