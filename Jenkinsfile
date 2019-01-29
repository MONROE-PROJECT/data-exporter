import java.text.SimpleDateFormat
jobName = "metadata-exporter"
version = "0.1.54"
build_dir = "build"
buildPackageName = "meta-exporter"

node ('dockerslave') {
    // Be sure that workspace is cleaned
    deleteDir()
    stage ('Git') {
        git branch: 'master', url: 'git@github.com:MONROE-PROJECT/Utilities.git'

        checkout([$class: 'GitSCM',
            branches: [[name: 'master']],
            doGenerateSubmoduleConfigurations: false,
            extensions: [[$class: 'RelativeTargetDirectory', relativeTargetDir: 'metadata-exporter-alt']],
            submoduleCfg: [],
            userRemoteConfigs: [[url: 'git@github.com:MONROE-PROJECT/data-exporter.git']]])
        gitCommit = sh(returnStdout: true, script: 'git rev-parse HEAD').trim()
        shortCommit = gitCommit.take(6)
        commitChangeset = sh(returnStdout: true, script: 'git diff-tree --no-commit-id --name-status -r HEAD').trim()
        commitMessage = sh(returnStdout: true, script: "git show ${gitCommit} --format=%B --name-status").trim()
        sh """echo "${commitMessage}" > CHANGELIST"""
        def dateFormat = new SimpleDateFormat("yyyyMMddHHmm")
        def date = new Date()
        def timestamp = dateFormat.format(date).toString()

        checkout([$class: 'GitSCM',
                branches: [[name: 'monroe']],
                doGenerateSubmoduleConfigurations: false,
                extensions: [[$class: 'RelativeTargetDirectory', relativeTargetDir: 'versionize']],
                submoduleCfg: [],
                userRemoteConfigs: [[url: 'git@github.com:Celerway/celerway-jenkins.git']]])
    }

    stage ('Build') {
        dir(build_dir) {
            sh "cmake ../metadata-exporter-alt -DNNE=1 -DSQLITE3=1 -DZEROMQ_INPUT=1 -DZEROMQ_WRITER=1 -DZEROMQ_RELAY=1 -DGPS_NSB=1 -DMUNIN=1 -DSYSEVENT=1 -DNEAT=1 && make && make package"
        }
        sh "chmod +x versionize/versionize.sh; cp versionize/versionize.sh build/"
        dir(build_dir) {
            sh "mv meta_exporter-0.1.0-Linux.deb meta-exporter-0.1.0-Linux.deb"
            sh "./versionize.sh meta-exporter-0.1.0-Linux.deb ${buildPackageName} ${version} ${shortCommit} || true"
            sh "rm meta-exporter-0.1.0-Linux.deb"
        }
    }

    stage ('Configure') {
        dir(build_dir) {
            sh """echo `cat pk_${buildPackageName}/DEBIAN/control |grep Version|sed -e 's/Version: //'` > pkgver"""
        }
        sh """cp -an metadata-exporter/* build/pk_${buildPackageName}/"""
        dir(build_dir) {
            sh """sed -i -e 's/${buildPackageName}/metadata-exporter/g' pk_${buildPackageName}/DEBIAN/md5sums pk_${buildPackageName}/DEBIAN/control"""
            sh """mkdir -p pk_${buildPackageName}/usr/sbin && mv pk_${buildPackageName}/usr/sbin/meta_exporter pk_${buildPackageName}/usr/sbin/metadata-exporter"""
            sh '''PKGVER=`cat pkgver` ;dpkg -b pk_meta-exporter metadata-exporter-${PKGVER}-Linux.deb'''
            sh "rm meta-exporter*.deb"
        }
    }

    stage ('Archive artifacts') {
        archiveArtifacts "${build_dir}/*.deb"
    }
}
