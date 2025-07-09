pipeline {
    agent any
    
    environment {
        ZEPHYR_TOOLCHAIN_VARIANT = 'zephyr'
        ZEPHYR_SDK_VERSION       = '0.16.3'
        ZEPHYR_SDK_INSTALL_DIR   = "${WORKSPACE}/zephyr-sdk-${ZEPHYR_SDK_VERSION}"
        NRF_SDK_VERSION          = 'v2.6.0'
        NRF_SDK_PATH             = "${WORKSPACE}/ncs"
        BOARD                    = 'nrf52840dk_nrf52840'
        APP_DIR                  = 'app'

    }

    stages {

        stage('Clean Workspace') {
            steps {
                cleanWs()
            }
        }

        stage('Install Zephyr SDK') {
            steps {
                script {
                    sh '''
                    sudo apt-get update
                    sudo apt-get install -y build-essential cmake ninja-build git python3-pip clang-tidy gcc-multilib
                    pip3 install west

                    wget -q https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64.tar.xz
                    tar xf zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64.tar.xz
                    ${ZEPHYR_SDK_INSTALL_DIR}/setup.sh -h -c
                    ls
                    '''
                }
            }
        }

        stage('Setup the application') {
            steps {
                script {
                    sh '''
                    mkdir ws
                    python3 -m venv --copies ws/.venv
                    . ws/.venv/bin/activate
                    pip3 install west

                    # Initialize workspace
                    west init -m git@github.com:TAREQ-TBZ/env_sensor.git --mr main ws
                    cd ws
                    west update

                    # Install additional requirements
                    pip3 install -r zephyr/scripts/requirements-base.txt
                    pip3 install -r zephyr/scripts/requirements-extras.txt
                    pip3 install -r zephyr/scripts/requirements-build-test.txt
                    pip3 install -r nrf/scripts/requirements-base.txt
                    pip3 install -r nrf/scripts/requirements-build.txt
                    pip3 install -r bootloader/mcuboot/scripts/requirements.txt
                    '''
                }
            }
        }

        stage('Build Zephyr Application') {
            steps {
                script {
                    sh '''
                    cd ${WORKSPACE}
                    . ws/.venv/bin/activate
                    cd ws
                    west build -b sham_nrf52833 application/app
                    '''
                }
            }
        }

        stage('Archive Artifacts') {
            steps {
                archiveArtifacts artifacts: 'build/zephyr/*.hex, build/zephyr/*.elf', fingerprint: true
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
