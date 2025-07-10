pipeline {
    agent any

    triggers {
        // This will poll the repository for any changes.
        // The real trigger will be the webhook from GitHub.
        pollSCM('')
    }

    stages {

        stage('Clean Workspace') {
            steps {
                cleanWs()
            }
        }

        stage('Checkout') {
            steps {
                checkout scm
            }
        }

        stage('Preflight Check') {
            steps {
                script {
                    sh 'ls -al'
                    sh 'git describe --tags'
                }
            }
        }


        stage('Install Zephyr SDK') {
            when {
                tag "v*.*.*" // Example: v1.0.0, v2.3.1, etc. Adjust the pattern to match your tags.
            }
            agent {
                dockerfile {
                    filename 'Dockerfile'
                    dir '.'
                    reuseNode true
                    additionalBuildArgs '--build-arg USER_UID=1000 --build-arg USER_GID=1000 --build-arg USER_NAME=jenkins'
                }
            }
            steps {
                script {
                    sh '''
                    pwd
                    ls
                    rm -rf ws
                    mkdir -p ws
                    west init -m https://github.com/TAREQ-TBZ/environmental_sensor.git --mr main ws
                    cd ws
                    ls
                    west update

                    pip install -r zephyr/scripts/requirements.txt
                    pip install -r nrf/scripts/requirements.txt
                    pip install -r bootloader/mcuboot/scripts/requirements.txt

                    ls
                    west build -b sham_nrf52833 application/app
                    ls build/app/zephyr/
                    '''
                }
            }
        }

        stage('Archive Artifacts') {
            steps {
                dir('ws/build/app/zephyr') {
                    archiveArtifacts artifacts: '*.hex, *.elf', fingerprint: true
                }
            }
        }
    }

    post {
        success {
            echo '✅ Build completed successfully!'
        }
        failure {
            echo '❌ Build failed. Please check the logs.'
        }
    }
}
