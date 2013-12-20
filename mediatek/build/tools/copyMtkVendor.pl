#!/usr/bin/perl

$targetProduct = shift(@ARGV);
$baseProject = shift(@ARGV);

if ($baseProject ne "") {
	chomp(my $curDir = `pwd`);

    if ((! -e "vendor/mediatek/${targetProduct}") && (-e "vendor/mediatek/${baseProject}")) {
    	print "Copy vendor folder for ${targetProduct} from ${baseProject}...\n";
    	system ("mkdir -p vendor/mediatek/${targetProduct}");
    	system ("rsync -av --exclude=.svn --exclude=.git --exclude=.cvs vendor/mediatek/${baseProject}/ vendor/mediatek/${targetProduct}/ > auto_sync_vendor.log 2>&1");
    	system ("rm -rf vendor/mediatek/${targetProduct}/artifacts/out/target/product/${targetProduct}") if (-e "vendor/mediatek/${targetProduct}/artifacts/out/target/product/${targetProduct}");
    	system ("mv -f vendor/mediatek/${targetProduct}/artifacts/out/target/product/${baseProject} vendor/mediatek/${targetProduct}/artifacts/out/target/product/${targetProduct}");
    }
}

exit 0;
